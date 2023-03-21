/* ide-workbench-session.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench-session"

#include "config.h"

#include "ide-application.h"
#include "ide-session.h"
#include "ide-session-item.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-private.h"
#include "ide-workspace-private.h"

static void
ide_workbench_addin_restore_session_cb (PeasExtensionSet *set,
                                        PeasPluginInfo   *plugin_info,
                                        GObject    *exten,
                                        gpointer          user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;
  IdeSession *session = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_SESSION (session));

  ide_workbench_addin_restore_session (addin, session);
}

void
_ide_workbench_addins_restore_session (IdeWorkbench     *self,
                                       PeasExtensionSet *addins,
                                       IdeSession       *session)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_SESSION (session));
  g_return_if_fail (PEAS_IS_EXTENSION_SET (addins));

  peas_extension_set_foreach (addins,
                              ide_workbench_addin_restore_session_cb,
                              session);

  IDE_EXIT;
}

gboolean
_ide_workbench_restore_workspaces (IdeWorkbench *self,
                                   IdeSession   *session,
                                   gint64        present_time,
                                   GType         expected_workspace)
{
  IdeWorkspace *active_window = NULL;
  gboolean ret = FALSE;
  guint n_items;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);

  n_items = ide_session_get_n_items (session);

  for (guint i = 0; i < n_items; i++)
    {
      IdeSessionItem *item = ide_session_get_item (session, i);

      if (g_strcmp0 (ide_session_item_get_module_name (item), "libide-gui") == 0)
        {
          const char *type_hint = ide_session_item_get_type_hint (item);
          GType type = type_hint ? g_type_from_name (type_hint) : G_TYPE_INVALID;

          if (type && g_type_is_a (type, IDE_TYPE_WORKSPACE))
            {
              IdeWorkspace *workspace;
              const char *id;
              gboolean is_active = FALSE;
              gboolean is_maximized = FALSE;
              int width = -1, height = -1;

              if (type == expected_workspace)
                ret = TRUE;

              id = ide_session_item_get_id (item);

              if (ide_session_item_has_metadata_with_type (item, "is-maximized", G_VARIANT_TYPE ("b")))
                ide_session_item_get_metadata (item, "is-maximized", "b", &is_maximized);

              if (ide_session_item_has_metadata_with_type (item, "is-active", G_VARIANT_TYPE ("b")))
                ide_session_item_get_metadata (item, "is-active", "b", &is_active);

              if (ide_session_item_has_metadata_with_type (item, "size", G_VARIANT_TYPE ("(ii)")))
                ide_session_item_get_metadata (item, "size", "(ii)", &width, &height);

              workspace = g_object_new (type,
                                        "application", IDE_APPLICATION_DEFAULT,
                                        "id", id,
                                        NULL);
              ide_workbench_add_workspace (self, workspace);

              if (width > -1 && height > -1)
                {
                  gtk_window_set_default_size (GTK_WINDOW (workspace), width, height);
                  _ide_workspace_set_ignore_size_setting (workspace, TRUE);
                }

              if (is_maximized)
                gtk_window_maximize (GTK_WINDOW (workspace));

              if (is_active)
                active_window = workspace;
              else
                gtk_window_present_with_time (GTK_WINDOW (workspace), present_time);
            }
        }
    }

  if (active_window)
    gtk_window_present_with_time (GTK_WINDOW (active_window), present_time);

  IDE_RETURN (ret);
}
