/* gb-workbench-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-workbench-actions"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gb-workbench.h"
#include "gb-workbench-actions.h"
#include "gb-workbench-private.h"

static void
gb_workbench_actions_build (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_workbench_build_async (self, FALSE, NULL, NULL, NULL);
}

static void
gb_workbench_actions_rebuild (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_workbench_build_async (self, TRUE, NULL, NULL, NULL);
}

static void
gb_workbench_actions_new_document (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_workbench_add_temporary_buffer (self);
}

static void
gb_workbench_actions_global_search (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_box));
}

static void
gb_workbench_actions_open_uri_list (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;
  const gchar **uri_list;

  g_assert (GB_IS_WORKBENCH (self));

  uri_list = g_variant_get_strv (parameter, NULL);

  if (uri_list != NULL)
    {
      gb_workbench_open_uri_list (self, (const gchar * const *)uri_list);
      g_free (uri_list);
    }
}

static void
gb_workbench_actions_open_response (GtkFileChooser *chooser,
                                    gint            response_id,
                                    gpointer        user_data)
{
  g_autoptr(GbWorkbench) self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_hide (GTK_WIDGET (chooser));

  switch (response_id)
    {
    case GTK_RESPONSE_OK:
      {
        GSList *files;
        GSList *iter;
        gchar *file_uri;
        gchar *uri;

        file_uri = gtk_file_chooser_get_uri (chooser);
        uri = g_path_get_dirname (file_uri);
        if (g_strcmp0 (self->current_folder_uri, uri) != 0)
          {
            g_free (self->current_folder_uri);
            self->current_folder_uri = uri;
            uri = NULL;
          }
        g_free (uri);
        g_free (file_uri);

        files = gtk_file_chooser_get_files (chooser);
        for (iter = files; iter; iter = iter->next)
          {
            gb_workbench_open (self, G_FILE (iter->data));
            g_clear_object (&iter->data);
          }
        g_slist_free (files);
      }
      break;

    case GTK_RESPONSE_CANCEL:
    default:
      break;
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
gb_workbench_actions_open (GSimpleAction *action,
                           GVariant      *param,
                           gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkDialog *dialog;
  GtkWidget *suggested;

  g_assert (GB_IS_WORKBENCH (self));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "local-only", FALSE,
                         "modal", TRUE,
                         "select-multiple", TRUE,
                         "show-hidden", FALSE,
                         "transient-for", self,
                         "title", _("Open Document"),
                         NULL);

  if (self->current_folder_uri != NULL)
    gtk_file_chooser_set_current_folder_uri  (GTK_FILE_CHOOSER (dialog), self->current_folder_uri);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (gb_workbench_actions_open_response),
                    g_object_ref (self));

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gb_workbench_actions_save_all (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  GbWorkbench *self = user_data;
  IdeBufferManager *buffer_manager;

  g_assert (GB_IS_WORKBENCH (self));

  buffer_manager = ide_context_get_buffer_manager (self->context);
  ide_buffer_manager_save_all_async (buffer_manager, NULL, NULL, NULL);
}

static void
save_all_quit_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GbWorkbench) self = user_data;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /* TODO: We should find a way to propagate error info back.
   *       Right now, save_all doesn't.
   */

  ide_buffer_manager_save_all_finish (buffer_manager, result, NULL);
  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_workbench_actions_save_all_quit (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;
  IdeBufferManager *buffer_manager;

  g_assert (GB_IS_WORKBENCH (self));

  buffer_manager = ide_context_get_buffer_manager (self->context);
  ide_buffer_manager_save_all_async (buffer_manager, NULL, save_all_quit_cb, g_object_ref (self));
}

static void
gb_workbench_actions_nighthack (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", TRUE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder-dark");
}

static void
gb_workbench_actions_dayhack (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  g_autoptr(GSettings) settings = NULL;

  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", FALSE,
                NULL);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_set_string (settings, "style-scheme-name", "builder");
}

static void
gb_workbench_actions_search_docs (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
#if 0
  GbWorkbench *self = user_data;
  const gchar *str;

  g_return_if_fail (GB_IS_WORKBENCH (self));

  str = g_variant_get_string (parameter, NULL);
  gb_editor_workspace_search_help (self->editor_workspace, str);
#endif
}

static void
gb_workbench_actions_show_gear_menu (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->gear_menu_button), TRUE);
}

static void
gb_workbench_actions_show_left_pane (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *left_pane;
  gboolean reveal = FALSE;

  g_assert (GB_IS_WORKBENCH (self));

  left_pane = gb_workspace_get_left_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), left_pane,
                           "reveal", &reveal,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (self->workspace), left_pane,
                           "reveal", !reveal,
                           NULL);
}

static void
gb_workbench_actions_show_right_pane (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *right_pane;
  gboolean reveal = FALSE;

  g_assert (GB_IS_WORKBENCH (self));

  right_pane = gb_workspace_get_right_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), right_pane,
                           "reveal", &reveal,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (self->workspace), right_pane,
                           "reveal", !reveal,
                           NULL);
}

static void
gb_workbench_actions_show_bottom_pane (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *bottom_pane;
  gboolean reveal = FALSE;

  g_assert (GB_IS_WORKBENCH (self));

  bottom_pane = gb_workspace_get_bottom_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), bottom_pane,
                           "reveal", &reveal,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (self->workspace), bottom_pane,
                           "reveal", !reveal,
                           NULL);
}

static void
gb_workbench_actions_toggle_panels (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *left;
  GtkWidget *right;
  GtkWidget *bottom;
  gboolean reveal_left;
  gboolean reveal_right;
  gboolean reveal_bottom;

  g_assert (GB_IS_WORKBENCH (self));

  left = gb_workspace_get_left_pane (self->workspace);
  right = gb_workspace_get_right_pane (self->workspace);
  bottom = gb_workspace_get_bottom_pane (self->workspace);

  gtk_container_child_get (GTK_CONTAINER (self->workspace), left,
                           "reveal", &reveal_left,
                           NULL);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), right,
                           "reveal", &reveal_right,
                           NULL);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), bottom,
                           "reveal", &reveal_bottom,
                           NULL);

  if (reveal_left || reveal_right || reveal_bottom)
    {
      self->reveal_left_in_show = reveal_left;
      self->reveal_right_in_show = reveal_right;
      self->reveal_bottom_in_show = reveal_bottom;

      gtk_container_child_set (GTK_CONTAINER (self->workspace), left,
                               "reveal", FALSE,
                               NULL);
      gtk_container_child_set (GTK_CONTAINER (self->workspace), right,
                               "reveal", FALSE,
                               NULL);
      gtk_container_child_set (GTK_CONTAINER (self->workspace), bottom,
                               "reveal", FALSE,
                               NULL);
    }
  else
    {
      if (!self->reveal_left_in_show && !self->reveal_right_in_show && !self->reveal_bottom_in_show)
        {
          self->reveal_bottom_in_show = TRUE;
          self->reveal_left_in_show = TRUE;
          self->reveal_right_in_show = TRUE;
        }

      gtk_container_child_set (GTK_CONTAINER (self->workspace), left,
                               "reveal", self->reveal_left_in_show,
                               NULL);
      gtk_container_child_set (GTK_CONTAINER (self->workspace), right,
                               "reveal", self->reveal_right_in_show,
                               NULL);
      gtk_container_child_set (GTK_CONTAINER (self->workspace), bottom,
                               "reveal", self->reveal_bottom_in_show,
                               NULL);
    }
}

static void
sync_reveal_state (GtkWidget     *child,
                   GParamSpec    *pspec,
                   GSimpleAction *action)
{
  gboolean reveal = FALSE;

  g_assert (GB_IS_WORKSPACE_PANE (child));
  g_assert (pspec != NULL);
  g_assert (G_IS_SIMPLE_ACTION (action));

  gtk_container_child_get (GTK_CONTAINER (gtk_widget_get_parent (child)), child,
                           "reveal", &reveal,
                           NULL);
  g_simple_action_set_state (action, g_variant_new_boolean (reveal));
}

static void
gb_workbench_actions_focus_stack (GSimpleAction *action,
                                  GVariant      *variant,
                                  gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *stack;
  GList *stacks;
  gint nth;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32));

  /* Our index is 1-based for the column mapping. */
  nth = g_variant_get_int32 (variant);
  if (nth <= 0)
    return;

  stacks = gb_view_grid_get_stacks (self->view_grid);
  stack = g_list_nth_data (stacks, nth - 1);
  if (stack != NULL)
    gtk_widget_grab_focus (stack);
  g_list_free (stacks);
}

static gboolean
delayed_focus_timeout (gpointer data)
{
  GtkWidget *widget = data;

  if (gtk_widget_get_realized (widget))
    gtk_widget_grab_focus (widget);

  return G_SOURCE_REMOVE;
}

static void
gb_workbench_actions_focus_left (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *pane;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_WORKBENCH (self));

  pane = gb_workspace_get_left_pane (self->workspace);
  gtk_container_child_set (GTK_CONTAINER (self->workspace), pane,
                           "reveal", TRUE,
                           NULL);

  /* delay a bit in case widgets are in reveal */
  g_timeout_add_full (G_PRIORITY_LOW,
                      10,
                      delayed_focus_timeout,
                      g_object_ref (pane),
                      g_object_unref);
}

static void
gb_workbench_actions_focus_right (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbWorkbench *self = user_data;
  GtkWidget *pane;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_WORKBENCH (self));

  pane = gb_workspace_get_right_pane (self->workspace);
  gtk_container_child_set (GTK_CONTAINER (self->workspace), pane,
                           "reveal", TRUE,
                           NULL);

  /* delay a bit in case widgets are in reveal */
  g_timeout_add_full (G_PRIORITY_LOW,
                      10,
                      delayed_focus_timeout,
                      g_object_ref (pane),
                      g_object_unref);
}

static const GActionEntry GbWorkbenchActions[] = {
  { "build",            gb_workbench_actions_build },
  { "dayhack",          gb_workbench_actions_dayhack },
  { "focus-stack",      gb_workbench_actions_focus_stack, "i" },
  { "focus-left",       gb_workbench_actions_focus_left },
  { "focus-right",      gb_workbench_actions_focus_right },
  { "global-search",    gb_workbench_actions_global_search },
  { "new-document",     gb_workbench_actions_new_document },
  { "nighthack",        gb_workbench_actions_nighthack },
  { "open",             gb_workbench_actions_open },
  { "open-uri-list",    gb_workbench_actions_open_uri_list, "as" },
  { "rebuild",          gb_workbench_actions_rebuild },
  { "save-all",         gb_workbench_actions_save_all },
  { "save-all-quit",    gb_workbench_actions_save_all_quit },
  { "search-docs",      gb_workbench_actions_search_docs, "s" },
  { "show-gear-menu",   gb_workbench_actions_show_gear_menu },
  { "show-left-pane",   gb_workbench_actions_show_left_pane, NULL, "true" },
  { "show-right-pane",  gb_workbench_actions_show_right_pane, NULL, "false" },
  { "show-bottom-pane", gb_workbench_actions_show_bottom_pane, NULL, "false" },
  { "toggle-panels",    gb_workbench_actions_toggle_panels },
};

void
gb_workbench_actions_init (GbWorkbench *self)
{
  GSimpleActionGroup *actions;
  GAction *action;

  g_assert (GB_IS_WORKBENCH (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), GbWorkbenchActions,
                                   G_N_ELEMENTS (GbWorkbenchActions), self);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "build");
  g_object_bind_property (self, "building", action, "enabled",
                          (G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN));

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "rebuild");
  g_object_bind_property (self, "building", action, "enabled",
                          (G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN));

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "rebuild");
  g_object_bind_property (self, "building", action, "enabled",
                          (G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN));

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "show-left-pane");
  g_signal_connect_object (gb_workspace_get_left_pane (self->workspace),
                           "child-notify::reveal",
                           G_CALLBACK (sync_reveal_state),
                           action,
                           0);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "show-right-pane");
  g_signal_connect_object (gb_workspace_get_right_pane (self->workspace),
                           "child-notify::reveal",
                           G_CALLBACK (sync_reveal_state),
                           action,
                           0);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "show-bottom-pane");
  g_signal_connect_object (gb_workspace_get_bottom_pane (self->workspace),
                           "child-notify::reveal",
                           G_CALLBACK (sync_reveal_state),
                           action,
                           0);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "workbench", G_ACTION_GROUP (actions));
}
