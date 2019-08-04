/* ide-completion.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-completion"

#include "config.h"

#include <gtk/gtk.h>
#include <dazzle.h>
#include <gtksourceview/gtksource.h>
#include <libide-code.h>
#include <libide-plugins.h>
#include <libpeas/peas.h>
#include <string.h>

#ifdef GDK_WINDOWING_WAYLAND
# include <gdk/gdkwayland.h>
#endif

#include "ide-completion.h"
#include "ide-completion-context.h"
#include "ide-completion-display.h"
#include "ide-completion-overlay.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"

#include "ide-source-view-private.h"

#define DEFAULT_N_ROWS 5

struct _IdeCompletion
{
  GObject parent_instance;

  /*
   * The GtkSourceView that we are providing results for. This can be used by
   * providers to get a reference.
   */
  GtkSourceView *view;

  /*
   * A cancellable that we'll monitor to cancel anything that is currently in
   * flight. This is reset to a new GCancellable after each time
   * g_cancellable_cancel() is called.
   */
  GCancellable *cancellable;

  /*
   * Our extension manager to get providers that were registered by plugins.
   * We handle extension-added/extension-removed and add the results to the
   * @providers array so that we can allow manual adding of providers too.
   */
  IdeExtensionSetAdapter *addins;

  /*
   * An array of providers that have been registered. These will be queried
   * when input is provided for completion.
   */
  GPtrArray *providers;

  /*
   * If we are currently performing a completion, the context is stored here.
   * It will be cleared as soon as it's no longer valid to (re)display.
   */
  IdeCompletionContext *context;

  /*
   * The signal group is used to track changes to the context while it is our
   * current context. That includes handling notification of the first result
   * so that we can show the window, etc.
   */
  DzlSignalGroup *context_signals;

  /*
   * Signals to changes in the underlying GtkTextBuffer that we use to
   * determine where and how we can do completion.
   */
  DzlSignalGroup *buffer_signals;

  /*
   * We need to track various events on the view to ensure that we don't
   * activate at incorrect times.
   */
  DzlSignalGroup *view_signals;

  /*
   * The display for results. This may use a different implementation based on
   * the windowing system available to work around restrictions. For example,
   * on wayland or quartz we'd use a toplevel GtkOverlay to draw into where as
   * on Xorg we might just use an native window since we have more flexibility
   * in Move/Resize there.
   */
  IdeCompletionDisplay *display;

  /*
   * Our current event while processing so that we can get access to it
   * from a callback back into the completion instance.
   */
  const GdkEventKey *current_event;

  /*
   * Our cached font description to apply to views.
   */
  PangoFontDescription *font_desc;

  /*
   * If we have a queued update to refilter after deletions, this will be
   * set to the GSource id.
   */
  guint queued_update;

  /*
   * This value is incremented/decremented based on if we need to suppress
   * visibility of the completion window (and avoid doing queries).
   */
  guint block_count;

  /* Re-entrancy protection for ide_completion_show(). */
  guint showing;

  /*
   * The number of rows to display. This is propagated to the window if/when
   * the window is created.
   */
  guint n_rows;

  /* If we're currently being displayed */
  guint shown : 1;

  /* If we have a completion actively in play */
  guint waiting_for_results : 1;

  /* If we should refilter after the in-flight context completes */
  guint needs_refilter : 1;
};

G_DEFINE_TYPE (IdeCompletion, ide_completion, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_N_ROWS,
  PROP_VIEW,
  N_PROPS
};

enum {
  ACTIVATE,
  PROVIDER_ADDED,
  PROVIDER_REMOVED,
  SHOW,
  HIDE,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
ide_completion_is_blocked (IdeCompletion *self)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_COMPLETION (self));

  return self->block_count > 0 ||
         self->view == NULL ||
         self->providers->len == 0 ||
         !gtk_widget_get_visible (GTK_WIDGET (self->view)) ||
         !gtk_widget_has_focus (GTK_WIDGET (self->view)) ||
         !(buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view))) ||
         gtk_text_buffer_get_has_selection (buffer) ||
         !IDE_IS_SOURCE_VIEW (self->view) ||
         _ide_source_view_has_cursors (IDE_SOURCE_VIEW (self->view)) ||
         !ide_source_view_is_processing_key (IDE_SOURCE_VIEW (self->view));
}

static void
ide_completion_complete_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeCompletionContext *context = (IdeCompletionContext *)object;
  g_autoptr(IdeCompletion) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeCompletionDisplay *display;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_COMPLETION (self));

  if (self->context == context)
    self->waiting_for_results = FALSE;

  if (!_ide_completion_context_complete_finish (context, result, &error))
    {
      g_debug ("%s", error->message);
      IDE_EXIT;
    }

  if (context != self->context)
    IDE_EXIT;

  if (self->needs_refilter)
    {
      /*
       * At this point, we've gotten our new results for the context. But we had
       * new content come in since we fired that request. So we need to ask the
       * providers to further reduce the list based on updated query text.
       */
      self->needs_refilter = FALSE;
      _ide_completion_context_refilter (context);
    }

  display = ide_completion_get_display (self);

  if (!ide_completion_context_is_empty (context))
    gtk_widget_show (GTK_WIDGET (display));
  else
    gtk_widget_hide (GTK_WIDGET (display));

  IDE_EXIT;
}

static void
ide_completion_set_context (IdeCompletion        *self,
                            IdeCompletionContext *context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (!context || IDE_IS_COMPLETION_CONTEXT (context));

  if (g_set_object (&self->context, context))
    dzl_signal_group_set_target (self->context_signals, context);

  IDE_EXIT;
}

static inline gboolean
is_symbol_char (gunichar ch)
{
  return ch == '_' || g_unichar_isalnum (ch);
}

static gboolean
ide_completion_compute_bounds (IdeCompletion *self,
                               GtkTextIter   *begin,
                               GtkTextIter   *end)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  gunichar ch = 0;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, end, insert);

  *begin = *end;

  do
    {
      if (!gtk_text_iter_backward_char (begin))
        break;
      ch = gtk_text_iter_get_char (begin);
    }
  while (is_symbol_char (ch));

  if (ch && !is_symbol_char (ch))
    gtk_text_iter_forward_char (begin);

  if (GTK_SOURCE_IS_BUFFER (buffer))
    {
      GtkSourceBuffer *gsb = GTK_SOURCE_BUFFER (buffer);

      if (gtk_source_buffer_iter_has_context_class (gsb, begin, "comment") ||
          gtk_source_buffer_iter_has_context_class (gsb, begin, "string") ||
          gtk_source_buffer_iter_has_context_class (gsb, end, "comment") ||
          gtk_source_buffer_iter_has_context_class (gsb, end, "string"))
        return FALSE;
    }

  return !gtk_text_iter_equal (begin, end);
}

static void
ide_completion_start (IdeCompletion           *self,
                      IdeCompletionActivation  activation)
{
  g_autoptr(IdeCompletionContext) context = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (self->context == NULL);

  dzl_clear_source (&self->queued_update);

  if (!ide_completion_compute_bounds (self, &begin, &end))
    {
      if (activation == IDE_COMPLETION_INTERACTIVE)
        IDE_EXIT;
      begin = end;
    }

  context = _ide_completion_context_new (self);
  for (guint i = 0; i < self->providers->len; i++)
    _ide_completion_context_add_provider (context, g_ptr_array_index (self->providers, i));
  ide_completion_set_context (self, context);

  self->waiting_for_results = TRUE;
  self->needs_refilter = FALSE;

  _ide_completion_context_complete_async (context,
                                          activation,
                                          &begin,
                                          &end,
                                          self->cancellable,
                                          ide_completion_complete_cb,
                                          g_object_ref (self));

  if (self->display != NULL)
    {
      ide_completion_display_set_context (self->display, context);

      if (!ide_completion_context_is_empty (context))
        gtk_widget_show (GTK_WIDGET (self->display));
      else
        gtk_widget_hide (GTK_WIDGET (self->display));
    }

  IDE_EXIT;
}

static void
ide_completion_update (IdeCompletion           *self,
                       IdeCompletionActivation  activation)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (self->context != NULL);
  g_assert (IDE_IS_COMPLETION_CONTEXT (self->context));

  /*
   * First, find the boundary for the word we are trying to complete. We might
   * be able to refine a previous query instead of making a new one which can
   * save on a lot of backend work.
   */
  ide_completion_compute_bounds (self, &begin, &end);

  if (_ide_completion_context_can_refilter (self->context, &begin, &end))
    {
      IdeCompletionDisplay *display = ide_completion_get_display (self);

      /*
       * Make sure we update providers that have already delivered results
       * even though some of them won't be ready yet.
       */
      _ide_completion_context_refilter (self->context);

      /*
       * If we're waiting for the results still to come in, then just mark
       * that we need to do post-processing rather than trying to refilter now.
       */
      if (self->waiting_for_results)
        {
          self->needs_refilter = TRUE;
          IDE_EXIT;
        }

      if (!ide_completion_context_is_empty (self->context))
        gtk_widget_show (GTK_WIDGET (display));
      else
        gtk_widget_hide (GTK_WIDGET (display));

      IDE_EXIT;
    }

  if (!ide_completion_context_get_bounds (self->context, &begin, &end) ||
      gtk_text_iter_equal (&begin, &end))
    {
      if (activation == IDE_COMPLETION_INTERACTIVE)
        {
          ide_completion_hide (self);
          IDE_EXIT;
        }

      IDE_GOTO (reset);
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  /*
   * If our completion prefix bounds match the prefix that we looked
   * at previously, we can possibly refilter the previous context instead
   * of creating a new context.
   */

  /*
   * The context uses GtkTextMark which should have been advanced as
   * the user continued to type. So if @end matches @iter (our insert
   * location), then we can possibly update the previous context by
   * further refining the query to a subset of the result.
   */
  if (gtk_text_iter_equal (&iter, &end))
    {
      ide_completion_show (self);
      IDE_EXIT;
    }

reset:
  ide_completion_cancel (self);
  ide_completion_start (self, activation);

  IDE_EXIT;
}

static void
ide_completion_real_hide (IdeCompletion *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));

  if (self->display != NULL)
    gtk_widget_hide (GTK_WIDGET (self->display));

  IDE_EXIT;
}

static IdeCompletionDisplay *
ide_completion_create_display (IdeCompletion *self)
{
  GtkWidget *widget = GTK_WIDGET (self->view);
  GdkDisplay *display = gtk_widget_get_display (widget);

  if (FALSE) {}
#ifdef GDK_WINDOWING_WAYLAND
  else if (GDK_IS_WAYLAND_DISPLAY (display))
    return IDE_COMPLETION_DISPLAY (_ide_completion_overlay_new ());
#endif
#ifdef GDK_WINDOWING_QUARTZ
  /* Do string type check to avoid including obj-c header */
  else if (g_strcmp0 ("GdkQuartzDisplay", G_OBJECT_TYPE_NAME (display)) == 0)
    return IDE_COMPLETION_DISPLAY (_ide_completion_overlay_new ());
#endif
  else
    return IDE_COMPLETION_DISPLAY (_ide_completion_window_new (widget));
}

static void
ide_completion_real_show (IdeCompletion *self)
{
  IdeCompletionDisplay *display;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));

  display = ide_completion_get_display (self);

  if (self->context == NULL)
    ide_completion_start (self, IDE_COMPLETION_USER_REQUESTED);
  else
    ide_completion_update (self, IDE_COMPLETION_USER_REQUESTED);

  ide_completion_display_set_context (display, self->context);

  if (!ide_completion_context_is_empty (self->context))
    gtk_widget_show (GTK_WIDGET (display));
  else
    gtk_widget_hide (GTK_WIDGET (display));

  IDE_EXIT;
}

static void
ide_completion_notify_context_empty_cb (IdeCompletion        *self,
                                        GParamSpec           *pspec,
                                        IdeCompletionContext *context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  if (context != self->context)
    IDE_EXIT;

  if (ide_completion_context_is_empty (context))
    {
      if (self->display != NULL)
        gtk_widget_hide (GTK_WIDGET (self->display));
    }
  else
    {
      IdeCompletionDisplay *display = ide_completion_get_display (self);

      gtk_widget_show (GTK_WIDGET (display));
    }

  IDE_EXIT;
}

static gboolean
ide_completion_view_button_press_event_cb (IdeCompletion  *self,
                                           GdkEventButton *event,
                                           GtkSourceView  *view)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (self->view == view);

  ide_completion_hide (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_completion_view_focus_out_event_cb (IdeCompletion *self,
                                        GdkEventFocus *event,
                                        GtkSourceView *view)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (self->view == view);

  ide_completion_hide (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_completion_view_key_press_event_cb (IdeCompletion *self,
                                        GdkEventKey   *event,
                                        GtkSourceView *view)
{
  GtkBindingSet *binding_set;
  gboolean ret = GDK_EVENT_PROPAGATE;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (event != NULL);
  g_assert (event->type == GDK_KEY_PRESS);
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (self->view == view);

  binding_set = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (self));

  self->current_event = event;

  if (self->display != NULL &&
      gtk_widget_get_visible (GTK_WIDGET (self->display)) &&
      ide_completion_display_key_press_event (self->display, event))
    ret = GDK_EVENT_STOP;

  self->current_event = NULL;

  if (ret == GDK_EVENT_PROPAGATE)
    ret = gtk_binding_set_activate (binding_set, event->keyval, event->state, G_OBJECT (self));

  return ret;
}

static void
ide_completion_view_move_cursor_cb (IdeCompletion   *self,
                                    GtkMovementStep  step,
                                    gint             count,
                                    gboolean         extend_selection,
                                    GtkSourceView   *view)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  /* TODO: Should we keep the context alive while we begin a new one?
   *       Or rather, how can we avoid the hide/show of the widget that
   *       could result in flicker?
   */

  if (self->display != NULL &&
      gtk_widget_get_visible (GTK_WIDGET (self->display)))
    ide_completion_cancel (self);
}

static gboolean
ide_completion_queued_update_cb (gpointer user_data)
{
  IdeCompletion *self = user_data;

  g_assert (IDE_IS_COMPLETION (self));

  self->queued_update = 0;

  if (self->context != NULL)
    ide_completion_update (self, IDE_COMPLETION_INTERACTIVE);

  return G_SOURCE_REMOVE;
}

static void
ide_completion_queue_update (IdeCompletion *self)
{
  g_assert (IDE_IS_COMPLETION (self));

  dzl_clear_source (&self->queued_update);

  /*
   * We hit this code path when the user has deleted text. We want to
   * introduce just a bit of delay so that deleting under heavy key
   * repeat will not stall doing lots of refiltering.
   */

  self->queued_update =
    gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                  20,
                                  ide_completion_queued_update_cb,
                                  self,
                                  NULL);
}

static void
ide_completion_buffer_delete_range_after_cb (IdeCompletion *self,
                                             GtkTextIter   *begin,
                                             GtkTextIter   *end,
                                             GtkTextBuffer *buffer)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (self->context != NULL)
    {
      if (!ide_completion_is_blocked (self))
        {
          GtkTextIter b, e;

          ide_completion_context_get_bounds (self->context, &b, &e);

          /*
           * If they just backspaced all of the text, then we want to just hide
           * the completion window since that can get a bit intrusive.
           */
          if (gtk_text_iter_equal (&b, &e))
            {
              dzl_clear_source (&self->queued_update);
              ide_completion_hide (self);
              return;
            }

          ide_completion_queue_update (self);
        }
    }
}

static gboolean
is_single_char (const gchar *text,
                gint         len)
{
  if (len == 1)
    return TRUE;
  else if (len > 6)
    return FALSE;
  else
    return g_utf8_strlen (text, len) == 1;
}

static void
ide_completion_buffer_insert_text_after_cb (IdeCompletion *self,
                                            GtkTextIter   *iter,
                                            const gchar   *text,
                                            gint           len,
                                            GtkTextBuffer *buffer)
{
  IdeCompletionActivation activation = IDE_COMPLETION_INTERACTIVE;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (iter != NULL);
  g_assert (text != NULL);
  g_assert (len > 0);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (ide_buffer_get_loading (IDE_BUFFER (buffer)))
    return;

  dzl_clear_source (&self->queued_update);

  if (ide_completion_is_blocked (self) || !is_single_char (text, len))
    {
      ide_completion_cancel (self);
      return;
    }

  if (!ide_completion_compute_bounds (self, &begin, &end))
    {
      GtkTextIter cur = end;

      if (gtk_text_iter_backward_char (&cur))
        {
          gunichar ch = gtk_text_iter_get_char (&cur);

          for (guint i = 0; i < self->providers->len; i++)
            {
              IdeCompletionProvider *provider = g_ptr_array_index (self->providers, i);

              if (ide_completion_provider_is_trigger (provider, &end, ch))
                {
                  /*
                   * We got a trigger, but we failed to continue the bounds of a previous
                   * completion. We need to cancel the previous completion (if any) first
                   * and then try to start a new completion due to trigger.
                   */
                  ide_completion_cancel (self);
                  activation = IDE_COMPLETION_TRIGGERED;
                  goto do_completion;
                }
            }
        }

      ide_completion_cancel (self);
      return;
    }

do_completion:

  if (self->context == NULL)
    ide_completion_start (self, activation);
  else
    ide_completion_update (self, activation);
}

static void
ide_completion_buffer_mark_set_cb (IdeCompletion     *self,
                                   const GtkTextIter *iter,
                                   GtkTextMark       *mark,
                                   GtkTextBuffer     *buffer)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (GTK_IS_TEXT_MARK (mark));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (mark != gtk_text_buffer_get_insert (buffer))
    return;

  if (_ide_completion_context_iter_invalidates (self->context, iter))
    ide_completion_cancel (self);
}

static void
ide_completion_set_view (IdeCompletion *self,
                         GtkSourceView *view)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (!view || IDE_IS_SOURCE_VIEW (view));

  if (view == NULL)
    {
      g_critical ("%s created without a view", G_OBJECT_TYPE_NAME (self));
      return;
    }

  if (g_set_weak_pointer (&self->view, view))
    {
      dzl_signal_group_set_target (self->view_signals, view);
      g_object_bind_property (view, "buffer",
                              self->buffer_signals, "target",
                              G_BINDING_SYNC_CREATE);
    }
}

static void
ide_completion_addins_extension_added_cb (IdeExtensionSetAdapter *adapter,
                                          PeasPluginInfo         *plugin_info,
                                          PeasExtension          *exten,
                                          gpointer                user_data)
{
  IdeCompletionProvider *provider = (IdeCompletionProvider *)exten;
  IdeCompletion *self = user_data;
  GtkTextBuffer *buffer;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));

  if ((buffer = ide_completion_get_buffer (self)) && IDE_IS_BUFFER (buffer))
    {
      g_autoptr(IdeContext) context = ide_buffer_ref_context (IDE_BUFFER (buffer));
      _ide_completion_provider_load (provider, context);
    }

  ide_completion_add_provider (self, provider);

  IDE_EXIT;
}

static void
ide_completion_addins_extension_removed_cb (IdeExtensionSetAdapter *adapter,
                                            PeasPluginInfo         *plugin_info,
                                            PeasExtension          *exten,
                                            gpointer                user_data)
{
  IdeCompletionProvider *provider = (IdeCompletionProvider *)exten;
  IdeCompletion *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));

  ide_completion_remove_provider (self, provider);

  IDE_EXIT;
}

static void
ide_completion_buffer_signals_bind_cb (IdeCompletion   *self,
                                       GtkSourceBuffer *buffer,
                                       DzlSignalGroup  *group)
{
  GtkSourceLanguage *language;
  IdeObjectBox *box;
  const gchar *language_id = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  if (!IDE_IS_BUFFER (buffer))
    return;

  if ((language = gtk_source_buffer_get_language (buffer)))
    language_id = gtk_source_language_get_id (language);

  box = ide_object_box_from_object (G_OBJECT (buffer));
  self->addins = ide_extension_set_adapter_new (IDE_OBJECT (box),
                                                peas_engine_get_default (),
                                                IDE_TYPE_COMPLETION_PROVIDER,
                                                "Completion-Provider-Languages",
                                                language_id);

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (ide_completion_addins_extension_added_cb),
                           self, 0);
  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (ide_completion_addins_extension_removed_cb),
                           self, 0);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_completion_addins_extension_added_cb,
                                     self);

  IDE_EXIT;
}

static void
ide_completion_buffer_signals_unbind_cb (IdeCompletion   *self,
                                         DzlSignalGroup  *group)
{
  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  ide_clear_and_destroy_object (&self->addins);

  IDE_EXIT;
}

static void
ide_completion_buffer_notify_language_cb (IdeCompletion   *self,
                                          GParamSpec      *pspec,
                                          GtkSourceBuffer *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (pspec != NULL);
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

  if (self->addins != NULL)
    {
      GtkSourceLanguage *language;
      const gchar *language_id = NULL;

      if ((language = gtk_source_buffer_get_language (buffer)))
        language_id = gtk_source_language_get_id (language);

      ide_extension_set_adapter_set_value (self->addins, language_id);
    }

  IDE_EXIT;
}

static void
ide_completion_dispose (GObject *object)
{
  IdeCompletion *self = (IdeCompletion *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_COMPLETION (self));

  if (self->display != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->display));

  g_assert (self->display == NULL);

  dzl_signal_group_set_target (self->context_signals, NULL);
  dzl_signal_group_set_target (self->buffer_signals, NULL);
  dzl_signal_group_set_target (self->view_signals, NULL);

  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);

  if (self->providers->len > 0)
    g_ptr_array_remove_range (self->providers, 0, self->providers->len);

  G_OBJECT_CLASS (ide_completion_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_completion_finalize (GObject *object)
{
  IdeCompletion *self = (IdeCompletion *)object;

  IDE_ENTRY;

  dzl_clear_source (&self->queued_update);

  g_clear_object (&self->cancellable);
  ide_clear_and_destroy_object (&self->addins);
  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->context_signals);
  g_clear_object (&self->view_signals);
  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->providers, g_ptr_array_unref);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  g_clear_weak_pointer (&self->view);

  G_OBJECT_CLASS (ide_completion_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_completion_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeCompletion *self = IDE_COMPLETION (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_completion_get_buffer (self));
      break;

    case PROP_N_ROWS:
      g_value_set_uint (value, ide_completion_get_n_rows (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeCompletion *self = IDE_COMPLETION (object);

  switch (prop_id)
    {
    case PROP_N_ROWS:
      ide_completion_set_n_rows (self, g_value_get_uint (value));
      break;

    case PROP_VIEW:
      ide_completion_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_class_init (IdeCompletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->dispose = ide_completion_dispose;
  object_class->finalize = ide_completion_finalize;
  object_class->get_property = ide_completion_get_property;
  object_class->set_property = ide_completion_set_property;

  /**
   * IdeCompletion:buffer:
   *
   * The #GtkTextBuffer for the #IdeCompletion:view.
   * This is a convenience property for providers.
   *
   * Since: 3.32
   */
  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for the view",
                         GTK_TYPE_TEXT_VIEW,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeCompletion:n-rows:
   *
   * The number of rows to display to the user.
   *
   * Since: 3.32
   */
  properties [PROP_N_ROWS] =
    g_param_spec_uint ("n-rows",
                       "Number of Rows",
                       "Number of rows to display to the user",
                       1, 32, DEFAULT_N_ROWS,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * IdeCompletion:view:
   *
   * The "view" property is the #GtkTextView for which this #IdeCompletion
   * is providing completion features.
   *
   * Since: 3.32
   */
  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The text view for which to provide completion",
                         GTK_SOURCE_TYPE_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeCompletion::provider-added:
   * @self: an #ideCompletion
   * @provider: an #IdeCompletionProvider
   *
   * The "provided-added" signal is emitted when a new provider is
   * added to the completion.
   *
   * Since: 3.32
   */
  signals [PROVIDER_ADDED] =
    g_signal_new ("provider-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_COMPLETION_PROVIDER);
  g_signal_set_va_marshaller (signals [PROVIDER_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * IdeCompletion::provider-removed:
   * @self: an #ideCompletion
   * @provider: an #IdeCompletionProvider
   *
   * The "provided-removed" signal is emitted when a provider has
   * been removed from the completion.
   *
   * Since: 3.32
   */
  signals [PROVIDER_REMOVED] =
    g_signal_new ("provider-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_COMPLETION_PROVIDER);
  g_signal_set_va_marshaller (signals [PROVIDER_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * IdeCompletion::hide:
   * @self: an #IdeCompletion
   *
   * The "hide" signal is emitted when the completion window should
   * be hidden.
   *
   * Since: 3.32
   */
  signals [HIDE] =
    g_signal_new_class_handler ("hide",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_completion_real_hide),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [HIDE],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  /**
   * IdeCompletion::show:
   * @self: an #IdeCompletion
   *
   * The "show" signal is emitted when the completion window should
   * be shown.
   *
   * Since: 3.32
   */
  signals [SHOW] =
    g_signal_new_class_handler ("show",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_completion_real_show),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [SHOW],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK, "show", 0);
}

static void
ide_completion_init (IdeCompletion *self)
{
  self->cancellable = g_cancellable_new ();
  self->providers = g_ptr_array_new_with_free_func (g_object_unref);
  self->buffer_signals = dzl_signal_group_new (GTK_TYPE_TEXT_BUFFER);
  self->context_signals = dzl_signal_group_new (IDE_TYPE_COMPLETION_CONTEXT);
  self->view_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_VIEW);
  self->n_rows = DEFAULT_N_ROWS;

  /*
   * We want to be notified when the context switches from no results to
   * having results (or vice-versa, when we've filtered to the point of
   * no results).
   */
  dzl_signal_group_connect_object (self->context_signals,
                                   "notify::empty",
                                   G_CALLBACK (ide_completion_notify_context_empty_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  /*
   * We need to know when the buffer inserts or deletes text so that we
   * possibly start showing the results, or update our previous completion
   * request.
   */
  g_signal_connect_object (self->buffer_signals,
                           "bind",
                           G_CALLBACK (ide_completion_buffer_signals_bind_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->buffer_signals,
                           "unbind",
                           G_CALLBACK (ide_completion_buffer_signals_unbind_cb),
                           self,
                           G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "notify::language",
                                   G_CALLBACK (ide_completion_buffer_notify_language_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "delete-range",
                                   G_CALLBACK (ide_completion_buffer_delete_range_after_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "insert-text",
                                   G_CALLBACK (ide_completion_buffer_insert_text_after_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "mark-set",
                                   G_CALLBACK (ide_completion_buffer_mark_set_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  /*
   * We track some events on the view that owns our IdeCompletion instance so
   * that we can hide the window when it definitely should not be displayed.
   */
  dzl_signal_group_connect_object (self->view_signals,
                                   "button-press-event",
                                   G_CALLBACK (ide_completion_view_button_press_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->view_signals,
                                   "focus-out-event",
                                   G_CALLBACK (ide_completion_view_focus_out_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->view_signals,
                                   "key-press-event",
                                   G_CALLBACK (ide_completion_view_key_press_event_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->view_signals,
                                   "move-cursor",
                                   G_CALLBACK (ide_completion_view_move_cursor_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->view_signals,
                                   "paste-clipboard",
                                   G_CALLBACK (ide_completion_block_interactive),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->view_signals,
                                   "paste-clipboard",
                                   G_CALLBACK (ide_completion_unblock_interactive),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

/**
 * ide_completion_get_view:
 * @self: a #IdeCompletion
 *
 * Returns: (transfer none): an #GtkSourceView
 *
 * Since: 3.32
 */
GtkSourceView *
ide_completion_get_view (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), NULL);

  return self->view;
}

/**
 * ide_completion_get_buffer:
 * @self: a #IdeCompletion
 *
 * Returns: (transfer none): a #GtkTextBuffer
 *
 * Since: 3.32
 */
GtkTextBuffer *
ide_completion_get_buffer (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), NULL);

  return gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
}

IdeCompletion *
_ide_completion_new (GtkSourceView *view)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (view), NULL);

  return g_object_new (IDE_TYPE_COMPLETION,
                       "view", view,
                       NULL);
}

/**
 * ide_completion_add_provider:
 * @self: an #IdeCompletion
 * @provider: an #IdeCompletionProvider
 *
 * Adds an #IdeCompletionProvider to the list of providers to be queried
 * for completion results.
 *
 * Since: 3.32
 */
void
ide_completion_add_provider (IdeCompletion         *self,
                             IdeCompletionProvider *provider)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));

  g_ptr_array_add (self->providers, g_object_ref (provider));
  g_signal_emit (self, signals [PROVIDER_ADDED], 0, provider);

  IDE_EXIT;
}

/**
 * ide_completion_remove_provider:
 * @self: an #IdeCompletion
 * @provider: an #IdeCompletionProvider
 *
 * Removes an #IdeCompletionProvider previously added with
 * ide_completion_add_provider().
 *
 * Since: 3.32
 */
void
ide_completion_remove_provider (IdeCompletion         *self,
                                IdeCompletionProvider *provider)
{
  g_autoptr(IdeCompletionProvider) hold = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));

  hold = g_object_ref (provider);

  if (g_ptr_array_remove (self->providers, provider))
    g_signal_emit (self, signals [PROVIDER_REMOVED], 0, hold);

  IDE_EXIT;
}

/**
 * ide_completion_show:
 * @self: an #IdeCompletion
 *
 * Emits the "show" signal.
 *
 * When the "show" signal is emitted, the completion window will be
 * displayed if there are any results available.
 *
 * Since: 3.32
 */
void
ide_completion_show (IdeCompletion *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  if (ide_completion_is_blocked (self))
    IDE_EXIT;

  self->showing++;
  if (self->showing == 1)
    g_signal_emit (self, signals [SHOW], 0);
  self->showing--;

  IDE_EXIT;
}

/**
 * ide_completion_hide:
 * @self: an #IdeCompletion
 *
 * Emits the "hide" signal.
 *
 * When the "hide" signal is emitted, the completion window will be
 * dismissed.
 *
 * Since: 3.32
 */
void
ide_completion_hide (IdeCompletion *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  g_signal_emit (self, signals [HIDE], 0);

  IDE_EXIT;
}

void
ide_completion_cancel (IdeCompletion *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  /* Nothing can re-use in-flight results now */
  self->waiting_for_results = FALSE;
  self->needs_refilter = FALSE;

  if (self->context != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      ide_completion_set_context (self, NULL);

      if (self->display != NULL)
        {
          ide_completion_display_set_context (self->display, NULL);
          gtk_widget_hide (GTK_WIDGET (self->display));
        }
    }

  IDE_EXIT;
}

void
ide_completion_block_interactive (IdeCompletion *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  self->block_count++;

  ide_completion_cancel (self);

  IDE_EXIT;
}

void
ide_completion_unblock_interactive (IdeCompletion *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  self->block_count--;

  IDE_EXIT;
}

void
ide_completion_set_n_rows (IdeCompletion *self,
                           guint          n_rows)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (n_rows > 0);
  g_return_if_fail (n_rows <= 32);

  if (self->n_rows != n_rows)
    {
      self->n_rows = n_rows;
      if (self->display != NULL)
        ide_completion_display_set_n_rows (self->display, n_rows);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ROWS]);
    }

  IDE_EXIT;
}

guint
ide_completion_get_n_rows (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), 0);
  return self->n_rows;
}

void
_ide_completion_activate (IdeCompletion         *self,
                          IdeCompletionContext  *context,
                          IdeCompletionProvider *provider,
                          IdeCompletionProposal *proposal)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (context));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));
  g_return_if_fail (IDE_IS_COMPLETION_PROPOSAL (proposal));

  self->block_count++;
  ide_completion_provider_activate_poposal (provider, context, proposal, self->current_event);
  self->block_count--;

  IDE_EXIT;
}

void
_ide_completion_set_language_id (IdeCompletion *self,
                                 const gchar   *language_id)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (language_id != NULL);

  ide_extension_set_adapter_set_value (self->addins, language_id);

  IDE_EXIT;
}

/**
 * ide_completion_is_visible:
 * @self: a #IdeCompletion
 *
 * Checks if the completion display is visible.
 *
 * Returns: %TRUE if the display is visible
 *
 * Since: 3.32
 */
gboolean
ide_completion_is_visible (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), FALSE);

  if (self->display != NULL)
    return gtk_widget_get_visible (GTK_WIDGET (self->display));

  return FALSE;
}

/**
 * ide_completion_get_display:
 * @self: a #IdeCompletion
 *
 * Gets the display for completion.
 *
 * Returns: (transfer none): an #IdeCompletionDisplay
 *
 * Since: 3.32
 */
IdeCompletionDisplay *
ide_completion_get_display (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), NULL);

  if (self->display == NULL)
    {
      self->display = ide_completion_create_display (self);
      g_signal_connect (self->display,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->display);
      ide_completion_display_set_n_rows (self->display, self->n_rows);
      ide_completion_display_attach (self->display, self->view);
      _ide_completion_display_set_font_desc (self->display, self->font_desc);
      ide_completion_display_set_context (self->display, self->context);
    }

  return self->display;
}

void
ide_completion_move_cursor (IdeCompletion   *self,
                            GtkMovementStep  step,
                            gint             direction)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_COMPLETION (self));

  if (self->display != NULL)
    ide_completion_display_move_cursor (self->display, step, direction);

  IDE_EXIT;
}

void
_ide_completion_set_font_description (IdeCompletion              *self,
                                      const PangoFontDescription *font_desc)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  if (font_desc != self->font_desc)
    {
      pango_font_description_free (self->font_desc);
      self->font_desc = pango_font_description_copy (font_desc);

      /*
       * Work around issue where when a proposal provides "<b>markup</b>" and
       * the weight is set in the font description, the <b> markup will not
       * have it's weight respected. This seems to be happening because the
       * weight mask is getting set in pango_font_description_from_string()
       * even if the the value is set to normal. That matter is complicated
       * because PangoAttrFontDesc and PangoAttrWeight will both have the
       * same starting offset in the PangoLayout.
       *
       * https://bugzilla.gnome.org/show_bug.cgi?id=755968
       */
      if (PANGO_WEIGHT_NORMAL == pango_font_description_get_weight (self->font_desc))
        pango_font_description_unset_fields (self->font_desc, PANGO_FONT_MASK_WEIGHT);

      if (self->display != NULL)
        _ide_completion_display_set_font_desc (self->display, font_desc);
    }
}

/**
 * ide_completion_fuzzy_match:
 * @haystack: (nullable): the string to be searched.
 * @casefold_needle: A g_utf8_casefold() version of the needle.
 * @priority: (out) (allow-none): An optional location for the score of the match
 *
 * This helper function can do a fuzzy match for you giving a haystack and
 * casefolded needle. Casefold your needle using g_utf8_casefold() before
 * running the query.
 *
 * Score will be set with the score of the match upon success. Otherwise,
 * it will be set to zero.
 *
 * Returns: %TRUE if @haystack matched @casefold_needle, otherwise %FALSE.
 *
 * Since: 3.32
 */
gboolean
ide_completion_fuzzy_match (const gchar *haystack,
                            const gchar *casefold_needle,
                            guint       *priority)
{
  gint real_score = 0;

  if (haystack == NULL || haystack[0] == 0)
    return FALSE;

  for (; *casefold_needle; casefold_needle = g_utf8_next_char (casefold_needle))
    {
      gunichar ch = g_utf8_get_char (casefold_needle);
      gunichar chup = g_unichar_toupper (ch);
      const gchar *tmp;
      const gchar *downtmp;
      const gchar *uptmp;

      /*
       * Note that the following code is not really correct. We want
       * to be relatively fast here, but we also don't want to convert
       * strings to casefolded versions for querying on each compare.
       * So we use the casefold version and compare with upper. This
       * works relatively well since we are usually dealing with ASCII
       * for function names and symbols.
       */

      downtmp = strchr (haystack, ch);
      uptmp = strchr (haystack, chup);

      if (downtmp && uptmp)
        tmp = MIN (downtmp, uptmp);
      else if (downtmp)
        tmp = downtmp;
      else if (uptmp)
        tmp = uptmp;
      else
        return FALSE;

      /*
       * Here we calculate the cost of this character into the score.
       * If we matched exactly on the next character, the cost is ZERO.
       * However, if we had to skip some characters, we have a cost
       * of 2*distance to the character. This is necessary so that
       * when we add the cost of the remaining haystack, strings which
       * exhausted @casefold_needle score lower (higher priority) than
       * strings which had to skip characters but matched the same
       * number of characters in the string.
       */
      real_score += (tmp - haystack) * 2;

      /* Add extra cost if we matched by using toupper */
      if (*haystack == chup)
        real_score += 1;

      /*
       * Now move past our matching character so we cannot match
       * it a second time.
       */
      haystack = tmp + 1;
    }

  if (priority != NULL)
    *priority = real_score + strlen (haystack);

  return TRUE;
}

/**
 * ide_completion_fuzzy_highlight:
 * @haystack: the string to be highlighted
 * @casefold_query: the typed-text used to highlight @haystack
 *
 * This will add &lt;b&gt; tags around matched characters in @haystack
 * based on @casefold_query.
 *
 * Returns: a newly allocated string
 *
 * Since: 3.32
 */
gchar *
ide_completion_fuzzy_highlight (const gchar *haystack,
                                const gchar *casefold_query)
{
  static const gchar *begin = "<b>";
  static const gchar *end = "</b>";
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;
  gboolean element_open = FALSE;

  if (haystack == NULL || casefold_query == NULL)
    return g_strdup (haystack);

  ret = g_string_new (NULL);

  for (; *haystack; haystack = g_utf8_next_char (haystack))
    {
      str_ch = g_utf8_get_char (haystack);
      match_ch = g_utf8_get_char (casefold_query);

      if ((str_ch == match_ch) || (g_unichar_tolower (str_ch) == g_unichar_tolower (match_ch)))
        {
          if (!element_open)
            {
              g_string_append (ret, begin);
              element_open = TRUE;
            }

          g_string_append_unichar (ret, str_ch);

          /* TODO: We could seek to the next char and append in a batch. */
          casefold_query = g_utf8_next_char (casefold_query);
        }
      else
        {
          if (element_open)
            {
              g_string_append (ret, end);
              element_open = FALSE;
            }

          g_string_append_unichar (ret, str_ch);
        }
    }

  if (element_open)
    g_string_append (ret, end);

  return g_string_free (ret, FALSE);
}
