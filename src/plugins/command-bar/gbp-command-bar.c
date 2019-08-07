/* gbp-command-bar.c
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

#define G_LOG_DOMAIN "gbp-command-bar"

#include "config.h"

#include <libide-gui.h>

#include "gbp-command-bar.h"
#include "gbp-command-bar-private.h"
#include "gbp-command-bar-suggestion.h"

struct _GbpCommandBar
{
  DzlBin              parent_instance;
  DzlSuggestionEntry *entry;
  GtkRevealer        *revealer;
  guint               queued_dismiss;
};

G_DEFINE_TYPE (GbpCommandBar, gbp_command_bar, DZL_TYPE_BIN)

static void
replace_model (GbpCommandBar *self,
               GListModel    *model)
{
  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (!model || G_IS_LIST_MODEL (model));

  dzl_suggestion_entry_set_model (self->entry, model);
}

static gint
compare_commands (gconstpointer a,
                  gconstpointer b)
{
  gint a_prio = ide_command_get_priority (*(IdeCommand **)a);
  gint b_prio = ide_command_get_priority (*(IdeCommand **)b);

  if (a_prio < b_prio)
    return -1;
  if (a_prio > b_prio)
    return 1;
  else
    return 0;
}

static void
gbp_command_bar_complete_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeCommandManager *command_manager = (IdeCommandManager *)object;
  g_autoptr(GbpCommandBar) self = user_data;
  g_autoptr(GPtrArray) res = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListStore) store = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_COMMAND_MANAGER (command_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_COMMAND_BAR (self));

  if ((res = ide_command_manager_query_finish (command_manager, result, &error)))
    {
      g_ptr_array_sort (res, compare_commands);

      store = g_list_store_new (GBP_TYPE_COMMAND_BAR_SUGGESTION);

      for (guint i = 0; i < res->len; i++)
        {
          IdeCommand *command = g_ptr_array_index (res, i);
          g_autoptr(GbpCommandBarSuggestion) suggestion = gbp_command_bar_suggestion_new (command);

          g_list_store_append (store, suggestion);
        }
    }

  replace_model (self, G_LIST_MODEL (store));
}

static void
gbp_command_bar_changed_cb (GbpCommandBar      *self,
                            DzlSuggestionEntry *entry)
{
  IdeCommandManager *command_manager;
  IdeWorkspace *workspace;
  const gchar *text;
  IdeContext *context;

  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));

  text = dzl_suggestion_entry_get_typed_text (entry);

  if (!gtk_widget_has_focus (GTK_WIDGET (entry)) || ide_str_empty0 (text))
    {
      replace_model (self, NULL);
      return;
    }

  g_debug ("Command Bar: %s", text);

  context = ide_widget_get_context (GTK_WIDGET (self));
  command_manager = ide_command_manager_from_context (context);
  workspace = ide_widget_get_workspace (GTK_WIDGET (self));

  ide_command_manager_query_async (command_manager,
                                   workspace,
                                   text,
                                   NULL,
                                   gbp_command_bar_complete_cb,
                                   g_object_ref (self));
}

static gboolean
gbp_command_bar_queue_dismiss_cb (gpointer data)
{
  GbpCommandBar *self = data;

  g_assert (GBP_IS_COMMAND_BAR (self));

  self->queued_dismiss = 0;

  if (!gtk_widget_has_focus (GTK_WIDGET (self->entry)))
    {
      GtkWidget *popover = dzl_suggestion_entry_get_popover (self->entry);

      if (!gtk_widget_get_visible (popover))
        gbp_command_bar_dismiss (self);
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_command_bar_queue_dismiss (GbpCommandBar *self)
{
  g_assert (GBP_IS_COMMAND_BAR (self));

  g_clear_handle_id (&self->queued_dismiss, g_source_remove);
  self->queued_dismiss = gdk_threads_add_idle (gbp_command_bar_queue_dismiss_cb, self);
}

static gboolean
gbp_command_bar_focus_out_event_cb (GbpCommandBar      *self,
                                    GdkEventFocus      *focus,
                                    DzlSuggestionEntry *entry)
{
  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));

  gbp_command_bar_queue_dismiss (self);

  return GDK_EVENT_PROPAGATE;
}

static void
gbp_command_bar_child_revealed_cb (GbpCommandBar *self,
                                   GParamSpec    *pspec,
                                   GtkRevealer   *revealer)
{
  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (GTK_IS_REVEALER (revealer));

  if (gtk_revealer_get_child_revealed (revealer))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self->entry)))
        gtk_widget_grab_focus (GTK_WIDGET (self->entry));
    }
  else
    gtk_widget_hide (GTK_WIDGET (self));
}

static void
gbp_command_bar_run_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeCommand *command = (IdeCommand *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_COMMAND (command));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (!ide_command_run_finish (command, result, &error))
    ide_object_warning (command, "%s", error->message);

  ide_object_destroy (IDE_OBJECT (command));
}

static void
gbp_command_bar_activate_suggestion_cb (GbpCommandBar      *self,
                                        DzlSuggestionEntry *entry)
{
  DzlSuggestion *suggestion;

  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));

  if ((suggestion = dzl_suggestion_entry_get_suggestion (entry)))
    {
      GbpCommandBarSuggestion *cbs = GBP_COMMAND_BAR_SUGGESTION (suggestion);
      IdeCommand *command = gbp_command_bar_suggestion_get_command (cbs);
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (entry));
      IdeCommandManager *command_manager = ide_command_manager_from_context (context);

      if (ide_object_is_root (IDE_OBJECT (command)))
        ide_object_append (IDE_OBJECT (command_manager), IDE_OBJECT (command));

      ide_command_run_async (command,
                             NULL,
                             gbp_command_bar_run_cb,
                             NULL);
    }

  gbp_command_bar_dismiss (self);
}

static void
gbp_command_bar_hide_suggestions_cb (GbpCommandBar      *self,
                                     DzlSuggestionEntry *entry)
{
  g_assert (GBP_IS_COMMAND_BAR (self));
  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));

  if (gtk_widget_has_focus (GTK_WIDGET (entry)))
    gbp_command_bar_dismiss (self);
}

static void
position_popover_cb (DzlSuggestionEntry *entry,
                     GdkRectangle       *area,
                     gboolean           *is_absolute,
                     gpointer            user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DZL_IS_SUGGESTION_ENTRY (entry));
  g_assert (area != NULL);
  g_assert (is_absolute != NULL);

  dzl_suggestion_entry_default_position_func (entry, area, is_absolute, NULL);

  /* We want to slightly adjust the popover positioning so it looks like the
   * popover disappears into the entry. It makes the revealer out a bit less
   * jarring as we hide the entry/window.
   */
  area->x += 3;
  area->width -= 6;
  area->y += 3;
}

static void
gbp_command_bar_destroy (GtkWidget *widget)
{
  GbpCommandBar *self = (GbpCommandBar *)widget;

  g_clear_handle_id (&self->queued_dismiss, g_source_remove);

  GTK_WIDGET_CLASS (gbp_command_bar_parent_class)->destroy (widget);
}

static void
gbp_command_bar_class_init (GbpCommandBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = gbp_command_bar_destroy;

  gtk_widget_class_set_css_name (widget_class, "commandbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/command-bar/gbp-command-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCommandBar, entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCommandBar, revealer);
}

static void
gbp_command_bar_init (GbpCommandBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);

  g_signal_connect_object (self->revealer,
                           "notify::child-revealed",
                           G_CALLBACK (gbp_command_bar_child_revealed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "activate-suggestion",
                           G_CALLBACK (gbp_command_bar_activate_suggestion_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "hide-suggestions",
                           G_CALLBACK (gbp_command_bar_hide_suggestions_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "focus-out-event",
                           G_CALLBACK (gbp_command_bar_focus_out_event_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gbp_command_bar_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_suggestion_entry_set_position_func (self->entry,
                                          position_popover_cb,
                                          NULL,
                                          NULL);

  _gbp_command_bar_init_shortcuts (self);
}

void
gbp_command_bar_reveal (GbpCommandBar *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_COMMAND_BAR (self));

  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    {
      /* First clear reveal child so that we will fade in properly
       * when setting reveal child below.
       */
      gtk_revealer_set_reveal_child (self->revealer, FALSE);
      gtk_widget_show (GTK_WIDGET (self));
    }

  gtk_revealer_set_reveal_child (self->revealer, TRUE);

  /* We need to try to grab focus immediately (best effort) or there is
   * potential for input events to be delivered to the previously focused
   * widget. We can't do this until after setting reveal-child or we can
   * get warnings about the widget not being ready for events.
   */
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

void
gbp_command_bar_dismiss (GbpCommandBar *self)
{
  IdeWorkspace *workspace;
  IdeSurface *surface;
  IdePage *page;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_COMMAND_BAR (self));

  gtk_revealer_set_reveal_child (self->revealer, FALSE);
  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  surface = ide_workspace_get_visible_surface (workspace);
  page = ide_workspace_get_most_recent_page (workspace);

  if (page != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (page));
  else
    gtk_widget_child_focus (GTK_WIDGET (surface), GTK_DIR_TAB_FORWARD);

  gtk_entry_set_text (GTK_ENTRY (self->entry), "");
}
