/* ide-webkit-plugin.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-webkit-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <girepository.h>

#include <libide-core.h>
#include <libide-webkit-api.h>

#include "ide-webkit-page.h"
#include "ide-webkit-workspace.h"

_IDE_EXTERN void _ide_webkit_register_types (PeasObjectModule *module);

static gboolean
ide_webkit_plugin_show_help_cb (IdeApplication *app,
                                gpointer        user_data)
{
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(GFile) file = NULL;
  GtkWindowGroup *group;
  IdeWebkitPage *page;
  IdeWorkbench *workbench;
  IdeWorkspace *workspace;
  GtkWindow *window;

  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (app));

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (app))))
    IDE_RETURN (FALSE);

  if (!(group = gtk_window_get_group (window)))
    IDE_RETURN (FALSE);

  if (!IDE_IS_WORKBENCH (group))
    IDE_RETURN (FALSE);

  workbench = IDE_WORKBENCH (group);

  workspace = ide_webkit_workspace_new ();
  ide_workbench_add_workspace (workbench, workspace);

  page = ide_webkit_page_new ();
  ide_webkit_page_load_uri (page, "file://" PACKAGE_DOCDIR "/en/index.html");

  position = panel_position_new ();
  ide_workspace_add_page (workspace, IDE_PAGE (page), position);

  gtk_window_present (GTK_WINDOW (workspace));

  IDE_RETURN (TRUE);
}

void
_ide_webkit_register_types (PeasObjectModule *module)
{
#if !WEBKIT_CHECK_VERSION(2, 39, 6)
  WebKitWebContext *context;
#endif
  g_autoptr(GError) error = NULL;

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
  g_type_ensure (IDE_TYPE_WEBKIT_PAGE);

  if (!g_irepository_require (NULL,
                              PACKAGE_WEBKIT_GIR_NAME,
                              PACKAGE_WEBKIT_GIR_VERSION,
                              0, &error))
    g_warning ("%s", error->message);

#if !WEBKIT_CHECK_VERSION(2, 39, 6)
  context = webkit_web_context_get_default ();
#endif
#if !WEBKIT_CHECK_VERSION(2, 39, 5)
  webkit_web_context_set_sandbox_enabled (context, TRUE);
#endif
#if !WEBKIT_CHECK_VERSION(2, 39, 6)
  webkit_web_context_set_favicon_database_directory (context, NULL);
#endif

  g_signal_connect (IDE_APPLICATION_DEFAULT,
                    "show-help",
                    G_CALLBACK (ide_webkit_plugin_show_help_cb),
                    NULL);
}
