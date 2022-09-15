/* gbp-sessionui-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-sessionui-workbench-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-sessionui-workbench-addin.h"

struct _GbpSessionuiWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
  IdeSession   *session;
};

static void
gbp_sessionui_workbench_addin_load (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_sessionui_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->session);

  self->workbench = NULL;
}

static void
gbp_sessionui_workbench_addin_save_session (IdeWorkbenchAddin *addin,
                                            IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));

  g_set_object (&self->session, session);

  /* TODO: Collect workspaces that are created (specifically secondary workspaces). */
  /* TODO: Collect pages within workspace grids */
  /* TODO: Collect panes and their position within workspace panels */
  /* TODO: Add ide_page_save_state(IdePage*, IdeSession*) */
  /* TODO: Add ide_pane_save_state(IdePage*, IdeSession*) */
  /* TODO: Add hooks to move a PanelWidget upon adding to dock */
}

static void
gbp_sessionui_workbench_addin_restore_session (IdeWorkbenchAddin *addin,
                                               IdeSession        *session)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
}

static void
gbp_sessionui_workbench_addin_replace_state_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_sessionui_workbench_addin_unload_project_async (IdeWorkbenchAddin   *addin,
                                                    IdeProjectInfo      *project_info,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  GbpSessionuiWorkbenchAddin *self = (GbpSessionuiWorkbenchAddin *)addin;
  g_autoptr(GVariant) sessionv = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) file = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_SESSION (self->session));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_sessionui_workbench_addin_unload_project_async);

  context = ide_workbench_get_context (self->workbench);
  file = ide_context_cache_file (context, "session.gvariant", NULL);
  sessionv = ide_session_to_variant (self->session);
  bytes = g_variant_get_data_as_bytes (sessionv);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       cancellable,
                                       gbp_sessionui_workbench_addin_replace_state_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_sessionui_workbench_addin_unload_project_finish (IdeWorkbenchAddin  *addin,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SESSIONUI_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_sessionui_workbench_addin_load;
  iface->unload = gbp_sessionui_workbench_addin_unload;
  iface->save_session = gbp_sessionui_workbench_addin_save_session;
  iface->restore_session = gbp_sessionui_workbench_addin_restore_session;
  iface->unload_project_async = gbp_sessionui_workbench_addin_unload_project_async;
  iface->unload_project_finish = gbp_sessionui_workbench_addin_unload_project_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSessionuiWorkbenchAddin, gbp_sessionui_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_sessionui_workbench_addin_class_init (GbpSessionuiWorkbenchAddinClass *klass)
{
}

static void
gbp_sessionui_workbench_addin_init (GbpSessionuiWorkbenchAddin *self)
{
}
