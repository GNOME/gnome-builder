/* ide-completion.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-completion"

#include "config.h"

#include <gtk/gtk.h>
#include <dazzle.h>
#include <gtksourceview/gtksource.h>
#include <libpeas/peas.h>

#ifdef GDK_WINDOWING_WAYLAND
# include <gdk/gdkwayland.h>
#endif

#include "buffers/ide-buffer.h"
#include "completion/ide-completion.h"
#include "completion/ide-completion-context.h"
#include "completion/ide-completion-display.h"
#include "completion/ide-completion-overlay.h"
#include "completion/ide-completion-private.h"
#include "completion/ide-completion-proposal.h"
#include "completion/ide-completion-provider.h"

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
   * This value is incremented/decremented based on if we need to suppress
   * visibility of the completion window (and avoid doing queries).
   */
  guint block_count;

  /*
   * The number of rows to display. This is propagated to the window if/when
   * the window is created.
   */
  guint n_rows;

  /* If we're currently being displayed */
  guint shown : 1;
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
         !GTK_SOURCE_IS_VIEW (self->view) ||
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

  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_COMPLETION (self));

  if (!_ide_completion_context_complete_finish (context, result, &error))
    g_debug ("%s", error->message);

  if (error != NULL)
    g_debug ("Providered failed: %s", error->message);
}

static void
ide_completion_set_context (IdeCompletion        *self,
                            IdeCompletionContext *context)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (!context || IDE_IS_COMPLETION_CONTEXT (context));

  if (g_set_object (&self->context, context))
    dzl_signal_group_set_target (self->context_signals, context);
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

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (self->context == NULL);

  if (!ide_completion_compute_bounds (self, &begin, &end))
    {
      if (activation == IDE_COMPLETION_INTERACTIVE)
        return;
      begin = end;
    }

  context = _ide_completion_context_new (self);
  for (guint i = 0; i < self->providers->len; i++)
    _ide_completion_context_add_provider (context, g_ptr_array_index (self->providers, i));
  ide_completion_set_context (self, context);

  _ide_completion_context_complete_async (context,
                                          &begin,
                                          &end,
                                          self->cancellable,
                                          ide_completion_complete_cb,
                                          g_object_ref (self));
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

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (self->context != NULL);
  g_assert (IDE_IS_COMPLETION_CONTEXT (self->context));

  /*
   * First, find the boundary for the word we are trying to complete. We might
   * be able to refine a previous query instead of making a new one which can
   * save on a lot of backend work.
   */
  if (ide_completion_compute_bounds (self, &begin, &end))
    {
      if (_ide_completion_context_can_refilter (self->context, &begin, &end))
        {
          _ide_completion_context_refilter (self->context);

          if (self->display != NULL)
            {
              if (!ide_completion_context_is_empty (self->context))
                gtk_widget_show (GTK_WIDGET (self->display));
            }

          return;
        }
    }

  if (!ide_completion_context_get_bounds (self->context, &begin, &end) ||
      gtk_text_iter_equal (&begin, &end))
    {
      if (activation == IDE_COMPLETION_INTERACTIVE)
        {
          ide_completion_hide (self);
          return;
        }

      goto reset;
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
      return;
    }

reset:
  ide_completion_cancel (self);
  ide_completion_start (self, activation);
}

static void
ide_completion_real_hide (IdeCompletion *self)
{
  g_assert (IDE_IS_COMPLETION (self));

  if (self->display != NULL)
    gtk_widget_hide (GTK_WIDGET (self->display));
}

#if 0
#ifdef GDK_WINDOWING_WAYLAND
static gboolean
configure_hack_cb (GtkWidget         *window,
                   GdkEventConfigure *configure,
                   gpointer           user_data)
{
  gtk_widget_hide (window);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK (configure_hack_cb), NULL);
  gtk_widget_set_opacity (window, 1.0);
  return GDK_EVENT_PROPAGATE;
}
#endif
#endif

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

  g_assert (IDE_IS_COMPLETION (self));

  display = ide_completion_get_display (self);

  if (self->context == NULL)
    ide_completion_start (self, IDE_COMPLETION_USER_REQUESTED);

  ide_completion_display_set_context (display, self->context);

  if (!ide_completion_context_is_empty (self->context))
    gtk_widget_show (GTK_WIDGET (display));
}

static void
ide_completion_notify_context_empty_cb (IdeCompletion        *self,
                                        GParamSpec           *pspec,
                                        IdeCompletionContext *context)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  if (ide_completion_context_is_empty (context))
    ide_completion_hide (self);
  else
    ide_completion_show (self);
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

static void
ide_completion_buffer_delete_range_cb (IdeCompletion *self,
                                       GtkTextIter   *begin,
                                       GtkTextIter   *end,
                                       GtkTextBuffer *buffer)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (IDE_IS_SOURCE_VIEW (self->view));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (ide_completion_is_blocked (self))
    return;

  if (self->context != NULL)
    ide_completion_update (self, IDE_COMPLETION_INTERACTIVE);
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

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));

  if ((buffer = ide_completion_get_buffer (self)) && IDE_IS_BUFFER (buffer))
    {
      /* TODO: Remove this when no longer necessary */
      IdeContext *context = ide_buffer_get_context (IDE_BUFFER (buffer));
      _ide_completion_provider_load (provider, context);
    }

  ide_completion_add_provider (self, provider);
}

static void
ide_completion_addins_extension_removed_cb (IdeExtensionSetAdapter *adapter,
                                            PeasPluginInfo         *plugin_info,
                                            PeasExtension          *exten,
                                            gpointer                user_data)
{
  IdeCompletionProvider *provider = (IdeCompletionProvider *)exten;
  IdeCompletion *self = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (provider));

  ide_completion_remove_provider (self, provider);
}

static void
ide_completion_buffer_signals_bind_cb (IdeCompletion   *self,
                                       GtkSourceBuffer *buffer,
                                       DzlSignalGroup  *group)
{
  GtkSourceLanguage *language;
  const gchar *language_id = NULL;
  IdeContext *context;

  g_assert (IDE_IS_COMPLETION (self));
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  if (!IDE_IS_BUFFER (buffer))
    return;

  if ((language = gtk_source_buffer_get_language (buffer)))
    language_id = gtk_source_language_get_id (language);

  context = ide_buffer_get_context (IDE_BUFFER (buffer));

  self->addins = ide_extension_set_adapter_new (context,
                                                peas_engine_get_default (),
                                                IDE_TYPE_COMPLETION_PROVIDER,
                                                "Completion-Provider-Languages",
                                                language_id);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_completion_addins_extension_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_completion_addins_extension_removed_cb),
                    self);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_completion_addins_extension_added_cb,
                                     self);
}

static void
ide_completion_buffer_signals_unbind_cb (IdeCompletion   *self,
                                         DzlSignalGroup  *group)
{
  g_assert (IDE_IS_COMPLETION (self));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  g_clear_object (&self->addins);
}

static void
ide_completion_buffer_notify_language_cb (IdeCompletion   *self,
                                          GParamSpec      *pspec,
                                          GtkSourceBuffer *buffer)
{
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
}

static void
ide_completion_dispose (GObject *object)
{
  IdeCompletion *self = (IdeCompletion *)object;

  g_assert (IDE_IS_COMPLETION (self));

  dzl_signal_group_set_target (self->context_signals, NULL);
  dzl_signal_group_set_target (self->buffer_signals, NULL);
  dzl_signal_group_set_target (self->view_signals, NULL);

  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);

  if (self->providers->len > 0)
    g_ptr_array_remove_range (self->providers, 0, self->providers->len);

  G_OBJECT_CLASS (ide_completion_parent_class)->dispose (object);
}

static void
ide_completion_finalize (GObject *object)
{
  IdeCompletion *self = (IdeCompletion *)object;

  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->context_signals);
  g_clear_object (&self->view_signals);
  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->providers, g_ptr_array_unref);
  g_clear_weak_pointer (&self->view);

  G_OBJECT_CLASS (ide_completion_parent_class)->finalize (object);
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
   * Since: 3.30
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
   * Since: 3.30
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
   * Since: 3.30
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
   * Since: 3.30
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
   * Since: 3.30
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
   * Since: 3.30
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
   * Since: 3.30
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
                                   G_CALLBACK (ide_completion_buffer_delete_range_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->buffer_signals,
                                   "insert-text",
                                   G_CALLBACK (ide_completion_buffer_insert_text_after_cb),
                                   self,
                                   G_CONNECT_AFTER | G_CONNECT_SWAPPED);

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
 * Since: 3.30
 */
void
ide_completion_add_provider (IdeCompletion         *self,
                             IdeCompletionProvider *provider)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));

  g_ptr_array_add (self->providers, g_object_ref (provider));
  g_signal_emit (self, signals [PROVIDER_ADDED], 0, provider);
}

/**
 * ide_completion_remove_provider:
 * @self: an #IdeCompletion
 * @provider: an #IdeCompletionProvider
 *
 * Removes an #IdeCompletionProvider previously added with
 * ide_completion_add_provider().
 *
 * Since: 3.30
 */
void
ide_completion_remove_provider (IdeCompletion         *self,
                                IdeCompletionProvider *provider)
{
  g_autoptr(IdeCompletionProvider) hold = NULL;

  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));

  hold = g_object_ref (provider);

  if (g_ptr_array_remove (self->providers, provider))
    g_signal_emit (self, signals [PROVIDER_REMOVED], 0, hold);
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
 * Since: 3.30
 */
void
ide_completion_show (IdeCompletion *self)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  g_signal_emit (self, signals [SHOW], 0);
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
 * Since: 3.30
 */
void
ide_completion_hide (IdeCompletion *self)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  g_signal_emit (self, signals [HIDE], 0);
}

void
ide_completion_cancel (IdeCompletion *self)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  if (self->context != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      ide_completion_set_context (self, NULL);
    }

  if (self->display != NULL)
    {
      ide_completion_display_set_context (self->display, NULL);
      gtk_widget_hide (GTK_WIDGET (self->display));
    }
}

void
ide_completion_block_interactive (IdeCompletion *self)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  self->block_count++;

  ide_completion_cancel (self);
}

void
ide_completion_unblock_interactive (IdeCompletion *self)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  self->block_count--;
}

void
ide_completion_set_n_rows (IdeCompletion *self,
                           guint          n_rows)
{
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
  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (context));
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (provider));
  g_return_if_fail (IDE_IS_COMPLETION_PROPOSAL (proposal));

  self->block_count++;
  ide_completion_provider_activate_poposal (provider, context, proposal, self->current_event);
  self->block_count--;
}

void
_ide_completion_set_language_id (IdeCompletion *self,
                                 const gchar   *language_id)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));
  g_return_if_fail (language_id != NULL);

  ide_extension_set_adapter_set_value (self->addins, language_id);
}

/**
 * ide_completion_is_visible:
 * @self: a #IdeCompletion
 *
 * Checks if the completion display is visible.
 *
 * Returns: %TRUE if the display is visible
 *
 * Since: 3.30
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
 * Since: 3.30
 */
IdeCompletionDisplay *
ide_completion_get_display (IdeCompletion *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION (self), NULL);

  if (self->display == NULL)
    {
      self->display = ide_completion_create_display (self);
      ide_completion_display_set_n_rows (self->display, self->n_rows);
      ide_completion_display_attach (self->display, self->view);
    }

  return self->display;
}

void
ide_completion_move_cursor (IdeCompletion   *self,
                            GtkMovementStep  step,
                            gint             direction)
{
  g_return_if_fail (IDE_IS_COMPLETION (self));

  if (self->display != NULL)
    ide_completion_display_move_cursor (self->display, step, direction);
}
