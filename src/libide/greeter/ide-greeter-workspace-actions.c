/* ide-greeter-workspace-actions.c
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

#define G_LOG_DOMAIN "ide-greeter-workspace-actions"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-greeter-private.h"
#include "ide-greeter-workspace.h"

static void
ide_greeter_workspace_dialog_response (IdeGreeterWorkspace  *self,
                                       gint                  response_id,
                                       GtkFileChooserDialog *dialog)
{
  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;
      g_autoptr(GFile) project_file = NULL;
      GtkFileFilter *filter;

      project_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, project_file);

      if ((filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog))))
        {
          const gchar *module_name = g_object_get_data (G_OBJECT (filter), "MODULE_NAME");

          if (module_name != NULL)
            ide_project_info_set_build_system_hint (project_info, module_name);

          /* If this is a directory selection, then make sure we set the
           * directory on the project-info too. That way we don't rely on
           * it being set elsewhere (which could be a translated symlink path).
           */
          if (g_object_get_data (G_OBJECT (filter), "IS_DIRECTORY"))
            ide_project_info_set_directory (project_info, project_file);
        }

      ide_greeter_workspace_open_project (self, project_info);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ide_greeter_workspace_dialog_notify_filter (IdeGreeterWorkspace  *self,
                                            GParamSpec           *pspec,
                                            GtkFileChooserDialog *dialog)
{
  GtkFileFilter *filter;
  GtkFileChooserAction action;
  const gchar *title;

  g_assert (IDE_IS_GREETER_WORKSPACE (self));
  g_assert (pspec != NULL);
  g_assert (GTK_IS_FILE_CHOOSER_DIALOG (dialog));

  filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (dialog));

  if (filter && g_object_get_data (G_OBJECT (filter), "IS_DIRECTORY"))
    {
      action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      title = _("Select Project Folder");
    }
  else
    {
      action = GTK_FILE_CHOOSER_ACTION_OPEN;
      title = _("Select Project File");
    }

  gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), action);
  gtk_window_set_title (GTK_WINDOW (dialog), title);
}

static void
ide_greeter_workspace_actions_open (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  IdeGreeterWorkspace *self = user_data;
  GtkFileChooserDialog *dialog;
  GtkFileFilter *all_filter;
  const GList *list;
  gint64 last_priority = G_MAXINT64;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param == NULL);
  g_assert (IDE_IS_GREETER_WORKSPACE (self));

  list = peas_engine_get_plugin_list (peas_engine_get_default ());

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "transient-for", self,
                         "modal", TRUE,
                         "title", _("Select Project Folder"),
                         "visible", TRUE,
                         NULL);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Open"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect_object (dialog,
                           "notify::filter",
                           G_CALLBACK (ide_greeter_workspace_dialog_notify_filter),
                           self,
                           G_CONNECT_SWAPPED);

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, _("All Project Types"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  /* For testing with no plugins */
  if (list == NULL)
    gtk_file_filter_add_pattern (all_filter, "*");

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      const gchar *module_name = peas_plugin_info_get_module_name (plugin_info);
      GtkFileFilter *filter;
      const gchar *pattern;
      const gchar *content_type;
      const gchar *name;
      const gchar *priority;
      gchar **patterns;
      gchar **content_types;
      gint i;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      name = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Name");
      if (name == NULL)
        continue;

      pattern = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Pattern");
      content_type = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Content-Type");
      priority = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Priority");

      if (pattern == NULL && content_type == NULL)
        continue;

      patterns = g_strsplit (pattern ?: "", ",", 0);
      content_types = g_strsplit (content_type ?: "", ",", 0);

      filter = gtk_file_filter_new ();

      gtk_file_filter_set_name (filter, name);

      if (!ide_str_equal0 (module_name, "greeter"))
        g_object_set_data_full (G_OBJECT (filter), "MODULE_NAME", g_strdup (module_name), g_free);

      for (i = 0; patterns [i] != NULL; i++)
        {
          if (*patterns [i])
            {
              gtk_file_filter_add_pattern (filter, patterns [i]);
              gtk_file_filter_add_pattern (all_filter, patterns [i]);
            }
        }

      for (i = 0; content_types [i] != NULL; i++)
        {
          if (*content_types [i])
            {
              gtk_file_filter_add_mime_type (filter, content_types [i]);
              gtk_file_filter_add_mime_type (all_filter, content_types [i]);

              /* Helper so we can change the file chooser action to OPEN_DIRECTORY,
               * otherwise the user won't be able to choose a directory, it will
               * instead dive into the directory.
               */
              if (g_strcmp0 (content_types [i], "inode/directory") == 0)
                g_object_set_data (G_OBJECT (filter), "IS_DIRECTORY", GINT_TO_POINTER (1));
            }
        }

      gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

      /* Look at the priority to set the default file filter. */
      if (priority != NULL)
        {
          gint64 pval = g_ascii_strtoll (priority, NULL, 10);

          if (pval < last_priority)
            {
              gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
              last_priority = pval;
            }
        }

      g_strfreev (patterns);
      g_strfreev (content_types);
    }

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (ide_greeter_workspace_dialog_response),
                           self,
                           G_CONNECT_SWAPPED);

  /* If unset, set the default filter */
  if (last_priority == G_MAXINT64)
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                       ide_get_projects_dir ());

  ide_gtk_window_present (GTK_WINDOW (dialog));
}

static const GActionEntry actions[] = {
  { "open", ide_greeter_workspace_actions_open },
};

void
_ide_greeter_workspace_init_actions (IdeGreeterWorkspace *self)
{
  g_return_if_fail (IDE_IS_GREETER_WORKSPACE (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);
}
