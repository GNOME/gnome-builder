/* gbp-sysprof-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-sysprof-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-sysprof-page.h"
#include "gbp-sysprof-workbench-addin.h"

struct _GbpSysprofWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
gbp_sysprof_workbench_addin_open_async (IdeWorkbenchAddin   *addin,
                                        GFile               *file,
                                        const char          *content_type,
                                        int                  at_line,
                                        int                  at_line_offset,
                                        IdeBufferOpenFlags   flags,
                                        IdePanelPosition    *position,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  GbpSysprofPage *page;
  IdeWorkspace *workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  workspace = ide_workbench_get_current_workspace (self->workbench);
  page = gbp_sysprof_page_new_for_file (file);
  ide_workspace_add_page (workspace, IDE_PAGE (page), position);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sysprof_workbench_addin_open_async);
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_sysprof_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
gbp_sysprof_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                      GFile             *file,
                                      const char        *content_type,
                                      gint              *priority)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_FILE (file));
  g_assert (priority != NULL);

  if (ide_str_equal0 (content_type, "application/x-sysprof-capture"))
    {
      *priority = 0;
      return TRUE;
    }

  return FALSE;
}

static void
gbp_sysprof_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_sysprof_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSysprofWorkbenchAddin *self = (GbpSysprofWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sysprof_workbench_addin_load;
  iface->unload = gbp_sysprof_workbench_addin_unload;
  iface->can_open = gbp_sysprof_workbench_addin_can_open;
  iface->open_async = gbp_sysprof_workbench_addin_open_async;
  iface->open_finish = gbp_sysprof_workbench_addin_open_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSysprofWorkbenchAddin, gbp_sysprof_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sysprof_workbench_addin_class_init (GbpSysprofWorkbenchAddinClass *klass)
{
}

static void
gbp_sysprof_workbench_addin_init (GbpSysprofWorkbenchAddin *self)
{
}
