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
  g_autoptr(GPtrArray) ar = NULL;
  gsize i;

  g_assert (GB_IS_WORKBENCH (self));

  buffer_manager = ide_context_get_buffer_manager (self->context);
  ar = ide_buffer_manager_get_buffers (buffer_manager);

  for (i = 0; i < ar->len; i++)
    {
      IdeBuffer *buffer;
      IdeFile *file;

      buffer = g_ptr_array_index (ar, i);
      file = ide_buffer_get_file (buffer);

      if (file == NULL)
        continue;

      ide_buffer_manager_save_file_async (buffer_manager, buffer, file, NULL, NULL, NULL, NULL);
    }
}

static void
gb_workbench_actions_show_command_bar (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  GbWorkbench *self = user_data;

  g_assert (GB_IS_WORKBENCH (self));

  gb_command_bar_show (self->command_bar);
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
  GbWorkbench *self = user_data;
  const gchar *str;

  g_return_if_fail (GB_IS_WORKBENCH (self));

  str = g_variant_get_string (parameter, NULL);
  gb_editor_workspace_search_help (self->editor_workspace, str);
}

static const GActionEntry GbWorkbenchActions[] = {
  { "build",            gb_workbench_actions_build },
  { "dayhack",          gb_workbench_actions_dayhack },
  { "global-search",    gb_workbench_actions_global_search },
  { "nighthack",        gb_workbench_actions_nighthack },
  { "open",             gb_workbench_actions_open },
  { "open-uri-list",    gb_workbench_actions_open_uri_list, "as" },
  { "rebuild",          gb_workbench_actions_rebuild },
  { "save-all",         gb_workbench_actions_save_all },
  { "search-docs",      gb_workbench_actions_search_docs, "s" },
  { "show-command-bar", gb_workbench_actions_show_command_bar },
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

  gtk_widget_insert_action_group (GTK_WIDGET (self), "workbench", G_ACTION_GROUP (actions));
}
