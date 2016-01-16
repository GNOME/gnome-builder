/* ide-omni-search-entry.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-search-box"

#include <glib/gi18n.h>

#include "ide-macros.h"
#include "ide-omni-search-entry.h"
#include "ide-omni-search-display.h"

#define SHORT_DELAY_TIMEOUT_MSEC 20
#define LONG_DELAY_TIMEOUT_MSEC  50
#define LONG_DELAY_MAX_CHARS     3
#define RESULTS_PER_PROVIDER     7

struct _IdeOmniSearchEntry
{
  GtkEntry              parent_instance;

  /* Template references */
  IdeOmniSearchDisplay *display;
  GtkEntry             *entry;
  GtkPopover           *popover;

  guint                 delay_timeout;
  gboolean              has_results;
};

G_DEFINE_TYPE (IdeOmniSearchEntry, ide_omni_search_entry, GTK_TYPE_ENTRY)

enum {
  CLEAR_SEARCH,
  MOVE_NEXT_RESULT,
  MOVE_PREVIOUS_RESULT,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void ide_omni_search_entry_changed         (IdeOmniSearchEntry *self);
static void ide_omni_search_entry_popover_hide    (IdeOmniSearchEntry *self,
                                                   GtkPopover         *popover);

GtkWidget *
ide_omni_search_entry_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_SEARCH_ENTRY, NULL);
}

/**
 * ide_omni_search_entry_get_search_engine:
 * @self: An #IdeOmniSearchEntry.
 *
 * Gets the search engine to use with the current workbench.
 *
 * Returns: (transfer none): An #IdeSearchEngine.
 */
IdeSearchEngine *
ide_omni_search_entry_get_search_engine (IdeOmniSearchEntry *self)
{
  IdeWorkbench *workbench;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self), NULL);

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  if (workbench == NULL)
    return NULL;

  context = ide_workbench_get_context (workbench);
  if (context == NULL)
      return NULL;

  return ide_context_get_search_engine (context);
}

static void
ide_omni_search_entry_hide_popover (IdeOmniSearchEntry *self,
                                    gboolean            leave_entry)
{
  g_autofree gchar *text = NULL;
  IdeWorkbench *workbench;
  IdePerspective *perspective;
  gint position = 0;

  /*
   * Hiding the popover will cause the entry to get focus,
   * thereby selecting all available text. We don't want
   * that to happen.
   */
  g_signal_handlers_block_by_func (self, ide_omni_search_entry_changed, NULL);
  g_signal_handlers_block_by_func (self->popover, ide_omni_search_entry_popover_hide, self);

  if (!leave_entry)
    {
      text = g_strdup (gtk_entry_get_text (GTK_ENTRY (self)));
      position = gtk_editable_get_position (GTK_EDITABLE (self));
    }

  gtk_entry_set_text (GTK_ENTRY (self), "");
  gtk_widget_hide (GTK_WIDGET (self->popover));

  if (!leave_entry)
    {
      gtk_entry_set_text (GTK_ENTRY (self), text);
      gtk_editable_set_position (GTK_EDITABLE (self), position);
    }

  g_signal_handlers_unblock_by_func (self->popover, ide_omni_search_entry_popover_hide, self);
  g_signal_handlers_unblock_by_func (self, ide_omni_search_entry_changed, NULL);

  if (leave_entry)
    {
      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      perspective = ide_workbench_get_visible_perspective (workbench);
      gtk_widget_grab_focus (GTK_WIDGET (perspective));

      self->has_results = FALSE;
    }
}

static void
ide_omni_search_entry_clear_search (IdeOmniSearchEntry *self)
{
  ide_omni_search_entry_hide_popover (self, TRUE);
}

static void
ide_omni_search_entry_completed (IdeOmniSearchEntry *self,
                                 IdeSearchContext   *context)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_assert (IDE_IS_SEARCH_CONTEXT (context));

  if (ide_omni_search_display_get_count (self->display) == 0)
    {
      self->has_results = FALSE;

      ide_omni_search_entry_hide_popover (self, FALSE);
    }
  else
    {
      self->has_results = TRUE;

      gtk_widget_set_visible (GTK_WIDGET (self->popover), TRUE);
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self));
    }
}

static gboolean
ide_omni_search_entry_delay_cb (gpointer user_data)
{
  IdeOmniSearchEntry *self = user_data;
  IdeSearchEngine *search_engine;
  IdeSearchContext *context;
  const gchar *search_text;

  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));

  self->delay_timeout = 0;

  if (self->display)
    {
      context = ide_omni_search_display_get_context (self->display);
      if (context != NULL)
        ide_search_context_cancel (context);

      search_engine = ide_omni_search_entry_get_search_engine (self);
      search_text = gtk_entry_get_text (GTK_ENTRY (self));
      if (search_engine == NULL || search_text == NULL)
        return G_SOURCE_REMOVE;

      context = ide_search_engine_search (search_engine, search_text);
      g_signal_connect_object (context,
                               "completed",
                               G_CALLBACK (ide_omni_search_entry_completed),
                               self,
                               G_CONNECT_SWAPPED);
      ide_omni_search_display_set_context (self->display, context);
      ide_search_context_execute (context, search_text, RESULTS_PER_PROVIDER);
      g_object_unref (context);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_omni_search_entry_activate (IdeOmniSearchEntry *self)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));

  gtk_widget_activate (GTK_WIDGET (self->display));
  ide_omni_search_entry_hide_popover (self, TRUE);
}

static void
ide_omni_search_entry_changed (IdeOmniSearchEntry *self)
{
  const gchar *text;
  gboolean had_focus;
  guint position;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));

  text = gtk_entry_get_text (GTK_ENTRY (self));
  had_focus = gtk_widget_has_focus (GTK_WIDGET (self));
  position = gtk_editable_get_position (GTK_EDITABLE (self));

  /*
   * Showing the popover could steal focus, so reset the focus to the
   * entry and reset the position which might get mucked up by focus
   * changes.
   */
  if (had_focus)
    {
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self));
      gtk_editable_set_position (GTK_EDITABLE (self), position);
    }

  if (self->delay_timeout == 0)
    {
      guint delay_msec = SHORT_DELAY_TIMEOUT_MSEC;

      if (text != NULL)
        {
          if (strlen (text) <= LONG_DELAY_MAX_CHARS)
            delay_msec = LONG_DELAY_TIMEOUT_MSEC;

          self->delay_timeout = g_timeout_add (delay_msec,
                                               ide_omni_search_entry_delay_cb,
                                               self);
        }
    }
}

static void
ide_omni_search_entry_display_result_activated (IdeOmniSearchEntry   *self,
                                                IdeSearchResult      *result,
                                                IdeOmniSearchDisplay *display)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (display));

  ide_omni_search_entry_hide_popover (self, TRUE);
}

static gboolean
ide_omni_search_entry_popover_key_press_event (IdeOmniSearchEntry *self,
                                               GdkEventKey        *event,
                                               GtkPopover         *popover)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_POPOVER (popover));

  return GTK_WIDGET_GET_CLASS (self)->key_press_event (GTK_WIDGET (self), event);
}

static void
ide_omni_search_entry_popover_hide (IdeOmniSearchEntry *self,
                                    GtkPopover         *popover)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));
  g_assert (GTK_IS_POPOVER (popover));

  if (self->has_results)
    ide_omni_search_entry_hide_popover (self, TRUE);
}

static void
ide_omni_search_entry_move_next_result (IdeOmniSearchEntry *self)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));

  ide_omni_search_display_move_next_result (self->display);
}

static void
ide_omni_search_entry_move_previous_result (IdeOmniSearchEntry *self)
{
  g_assert (IDE_IS_OMNI_SEARCH_ENTRY (self));

  ide_omni_search_display_move_previous_result (self->display);
}

static void
ide_omni_search_entry_destroy (GtkWidget *widget)
{
  IdeOmniSearchEntry *self = (IdeOmniSearchEntry *)widget;

  ide_clear_source (&self->delay_timeout);
  g_clear_pointer ((GtkWidget **)&self->popover, gtk_widget_destroy);

  GTK_WIDGET_CLASS (ide_omni_search_entry_parent_class)->destroy (widget);
}

static void
ide_omni_search_entry_class_init (IdeOmniSearchEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  widget_class->destroy = ide_omni_search_entry_destroy;

  g_signal_override_class_handler ("activate",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_CALLBACK (ide_omni_search_entry_activate));

  signals [CLEAR_SEARCH] =
    g_signal_new_class_handler ("clear-search",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_omni_search_entry_clear_search),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [MOVE_NEXT_RESULT] =
    g_signal_new_class_handler ("move-next-result",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_omni_search_entry_move_next_result),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [MOVE_PREVIOUS_RESULT] =
    g_signal_new_class_handler ("move-previous-result",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_omni_search_entry_move_previous_result),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "clear-search", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0, "activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0, "activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0, "move-next-result", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0, "move-previous-result", 0);
}

static void
ide_omni_search_entry_init (IdeOmniSearchEntry *self)
{
  g_object_set (self,
                "max-width-chars", 50,
                "primary-icon-name", "edit-find-symbolic",
                "primary-icon-activatable", FALSE,
                "primary-icon-sensitive", FALSE,
                NULL);

  self->popover = g_object_new (GTK_TYPE_POPOVER,
                                "width-request", 500,
                                "relative-to", self,
                                "position", GTK_POS_BOTTOM,
                                NULL);

  g_signal_connect_object (self->popover,
                           "key-press-event",
                           G_CALLBACK (ide_omni_search_entry_popover_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->popover,
                           "hide",
                           G_CALLBACK (ide_omni_search_entry_popover_hide),
                           self,
                           G_CONNECT_SWAPPED);

  self->display = g_object_new (IDE_TYPE_OMNI_SEARCH_DISPLAY,
                                "visible", TRUE,
                                NULL);

  gtk_container_add (GTK_CONTAINER (self->popover), GTK_WIDGET (self->display));

  g_signal_connect (self,
                    "changed",
                    G_CALLBACK (ide_omni_search_entry_changed),
                    NULL);

  g_signal_connect_object (self->display,
                           "result-activated",
                           G_CALLBACK (ide_omni_search_entry_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
}
