/* gbp-web-browser-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-web-browser-workbench-addin"

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-webkit.h>

#include "gbp-web-browser-workbench-addin.h"

struct _GbpWebBrowserWorkbenchAddin
{
  GObject parent_instance;
  IdeWorkbench *workbench;
};

static gboolean
gbp_web_browser_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                          GFile             *file,
                                          const char        *content_type,
                                          int               *priority)
{
  g_autofree char *scheme = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_WEB_BROWSER_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_FILE (file));
  g_assert (priority != NULL);

  scheme = g_file_get_uri_scheme (file);

  if (ide_str_equal0 (scheme, "https") || ide_str_equal0 (scheme, "http"))
    {
      *priority = -1000;
      return TRUE;
    }

  if (g_content_type_is_a (content_type, "text/html"))
    {
      *priority = 1000;
      return TRUE;
    }

  if (g_content_type_is_a (content_type, "text/plain") ||
      g_content_type_is_a (content_type, "application/x-zerosize"))
    {
      *priority = 10000;
      return TRUE;
    }

  return FALSE;
}

static inline gboolean
can_use_workspace (IdeWorkspace *workspace)
{
  if (workspace != NULL)
    {
      GType type = G_OBJECT_TYPE (workspace);

      return g_type_is_a (type, IDE_TYPE_PRIMARY_WORKSPACE) ||
             g_type_is_a (type, IDE_TYPE_EDITOR_WORKSPACE);
    }

  return FALSE;
}

static void
find_suitable_workspace_cb (IdeWorkspace *workspace,
                            gpointer      data)
{
  IdeWorkspace **ptr = data;

  if (*ptr == NULL && can_use_workspace (workspace))
    *ptr = workspace;
}

static void
gbp_web_browser_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                            GFile               *file,
                                            const char          *content_type,
                                            int                  at_line,
                                            int                  at_line_offset,
                                            IdeBufferOpenFlags   flags,
                                            PanelPosition       *position,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpWebBrowserWorkbenchAddin *self = (GbpWebBrowserWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *uri = NULL;
  IdeWebkitPage *page;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_WEB_BROWSER_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_web_browser_workbench_addin_open_async);

  workspace = ide_workbench_get_current_workspace (self->workbench);
  if (!can_use_workspace (workspace))
    {
      workspace = NULL;
      ide_workbench_foreach_workspace (self->workbench,
                                       find_suitable_workspace_cb,
                                       &workspace);
    }

  if (workspace == NULL)
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  page = ide_webkit_page_new ();
  uri = g_file_get_uri (file);

  ide_workspace_add_page (workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));

  ide_webkit_page_load_uri (page, uri);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_web_browser_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                             GAsyncResult       *result,
                                             GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (result));
  g_assert (GBP_IS_WEB_BROWSER_WORKBENCH_ADDIN (addin));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_web_browser_workbench_addin_load (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GbpWebBrowserWorkbenchAddin *self = (GbpWebBrowserWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  IDE_EXIT;
}

static void
gbp_web_browser_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                        IdeWorkbench      *workbench)
{
  GbpWebBrowserWorkbenchAddin *self = (GbpWebBrowserWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->can_open = gbp_web_browser_workbench_addin_can_open;
  iface->open_async = gbp_web_browser_workbench_addin_open_async;
  iface->open_finish = gbp_web_browser_workbench_addin_open_finish;
  iface->load = gbp_web_browser_workbench_addin_load;
  iface->unload = gbp_web_browser_workbench_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWebBrowserWorkbenchAddin, gbp_web_browser_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_web_browser_workbench_addin_class_init (GbpWebBrowserWorkbenchAddinClass *klass)
{
}

static void
gbp_web_browser_workbench_addin_init (GbpWebBrowserWorkbenchAddin *self)
{
}
