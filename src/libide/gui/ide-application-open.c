/* ide-application-open.c
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

#define G_LOG_DOMAIN "ide-application-open"

#include "config.h"

#include "ide-application.h"
#include "ide-application-private.h"
#include "ide-primary-workspace.h"
#include "ide-workbench.h"

typedef struct
{
  IdeProjectInfo *project_info;
  IdeWorkbench   *workbench;
} LocateProjectByFile;

static void
locate_project_by_file (gpointer item,
                        gpointer user_data)
{
  LocateProjectByFile *lookup = user_data;
  IdeProjectInfo *project_info;
  IdeWorkbench *workbench = item;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (lookup != NULL);

  if (lookup->workbench != NULL)
    return;

  if (!(project_info = ide_workbench_get_project_info (workbench)))
    return;

  if (ide_project_info_equal (project_info, lookup->project_info))
    lookup->workbench = workbench;
}

static void
ide_application_open_project_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_workbench_load_project_finish (workbench, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_object_ref (workbench), g_object_unref);

  IDE_EXIT;
}

void
ide_application_open_project_async (IdeApplication      *self,
                                    IdeProjectInfo      *project_info,
                                    GType                workspace_type,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeWorkbench) workbench = NULL;
  LocateProjectByFile lookup = { project_info, NULL };
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));
  g_return_if_fail (workspace_type == G_TYPE_INVALID ||
                    g_type_is_a (workspace_type, IDE_TYPE_WORKSPACE));

  if (workspace_type == G_TYPE_INVALID)
    workspace_type = self->workspace_type;

  self->workspace_type = IDE_TYPE_PRIMARY_WORKSPACE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_application_open_project_async);

  /* Try to activate a previously opened workbench before creating
   * and loading the project in a new one.
   */
  ide_application_foreach_workbench (self, locate_project_by_file, &lookup);

  if (lookup.workbench != NULL)
    {
      ide_workbench_activate (lookup.workbench);
      ide_task_return_pointer (task,
                               g_object_ref (lookup.workbench),
                               g_object_unref);
      IDE_EXIT;
    }

  workbench = ide_workbench_new ();
  ide_application_add_workbench (self, workbench);

  ide_workbench_load_project_async (workbench,
                                    project_info,
                                    workspace_type,
                                    cancellable,
                                    ide_application_open_project_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_application_open_project_finish:
 * @self: a #IdeApplication
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Completes a request to open a project.
 *
 * The workbench containing the project is returned, which may be an existing
 * workbench if the project was already opened.
 *
 * Returns: (transfer full): an #IdeWorkbench or %NULL on failure and @error
 *   is set.
 */
IdeWorkbench *
ide_application_open_project_finish (IdeApplication  *self,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  IdeWorkbench *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
