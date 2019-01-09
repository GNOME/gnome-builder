/* ide-workbench-addin.c
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

#define G_LOG_DOMAIN "ide-workbench-addin"

#include "config.h"

#include "ide-workbench-addin.h"

G_DEFINE_INTERFACE (IdeWorkbenchAddin, ide_workbench_addin, G_TYPE_OBJECT)

static void ide_workbench_addin_real_open_at_async (IdeWorkbenchAddin   *self,
                                                    GFile               *file,
                                                    const gchar         *hint,
                                                    gint                 at_line,
                                                    gint                 at_line_offset,
                                                    IdeBufferOpenFlags   flags,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data);
static void ide_workbench_addin_real_open_async    (IdeWorkbenchAddin   *self,
                                                    GFile               *file,
                                                    const gchar         *hint,
                                                    IdeBufferOpenFlags   flags,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data);

static void
ide_workbench_addin_real_load_project_async (IdeWorkbenchAddin   *self,
                                             IdeProjectInfo      *project_info,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  ide_task_report_new_error (self, callback, user_data,
                             ide_workbench_addin_real_load_project_async,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Loading projects is not supported");
}

static gboolean
ide_workbench_addin_real_load_project_finish (IdeWorkbenchAddin  *self,
                                              GAsyncResult       *result,
                                              GError            **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_workbench_addin_real_unload_project_async (IdeWorkbenchAddin   *self,
                                               IdeProjectInfo      *project_info,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  ide_task_report_new_error (self, callback, user_data,
                             ide_workbench_addin_real_unload_project_async,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Unloading projects is not supported");
}

static gboolean
ide_workbench_addin_real_unload_project_finish (IdeWorkbenchAddin  *self,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_workbench_addin_real_open_async (IdeWorkbenchAddin   *self,
                                     GFile               *file,
                                     const gchar         *hint,
                                     IdeBufferOpenFlags   flags,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  IdeWorkbenchAddinInterface *iface;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  iface = IDE_WORKBENCH_ADDIN_GET_IFACE (self);

  if (iface->open_at_async == (gpointer)ide_workbench_addin_real_open_at_async)
    {
      ide_task_report_new_error (self, callback, user_data,
                                 ide_workbench_addin_real_open_async,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Opening files is not supported");
      return;
    }

  iface->open_at_async (self, file, hint, -1, -1, flags, cancellable, callback, user_data);
}

static void
ide_workbench_addin_real_open_at_async (IdeWorkbenchAddin   *self,
                                        GFile               *file,
                                        const gchar         *hint,
                                        gint                 at_line,
                                        gint                 at_line_offset,
                                        IdeBufferOpenFlags   flags,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeWorkbenchAddinInterface *iface;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  iface = IDE_WORKBENCH_ADDIN_GET_IFACE (self);

  if (iface->open_async == (gpointer)ide_workbench_addin_real_open_async)
    {
      ide_task_report_new_error (self, callback, user_data,
                                 ide_workbench_addin_real_open_at_async,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Opening files is not supported");
      return;
    }

  iface->open_async (self, file, hint, flags, cancellable, callback, user_data);
}

static gboolean
ide_workbench_addin_real_open_finish (IdeWorkbenchAddin  *self,
                                      GAsyncResult       *result,
                                      GError            **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_workbench_addin_default_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load_project_async = ide_workbench_addin_real_load_project_async;
  iface->load_project_finish = ide_workbench_addin_real_load_project_finish;
  iface->unload_project_async = ide_workbench_addin_real_unload_project_async;
  iface->unload_project_finish = ide_workbench_addin_real_unload_project_finish;
  iface->open_async = ide_workbench_addin_real_open_async;
  iface->open_at_async = ide_workbench_addin_real_open_at_async;
  iface->open_finish = ide_workbench_addin_real_open_finish;
}

void
ide_workbench_addin_load (IdeWorkbenchAddin *self,
                          IdeWorkbench      *workbench)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->load)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->load (self, workbench);
}

void
ide_workbench_addin_unload (IdeWorkbenchAddin *self,
                            IdeWorkbench      *workbench)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->unload)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->unload (self, workbench);
}

void
ide_workbench_addin_load_project_async (IdeWorkbenchAddin   *self,
                                        IdeProjectInfo      *project_info,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->load_project_async (self,
                                                            project_info,
                                                            cancellable,
                                                            callback,
                                                            user_data);
}

gboolean
ide_workbench_addin_load_project_finish (IdeWorkbenchAddin  *self,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->load_project_finish (self, result, error);
}

void
ide_workbench_addin_unload_project_async (IdeWorkbenchAddin   *self,
                                          IdeProjectInfo      *project_info,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->unload_project_async (self,
                                                              project_info,
                                                              cancellable,
                                                              callback,
                                                              user_data);
}

gboolean
ide_workbench_addin_unload_project_finish (IdeWorkbenchAddin  *self,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->unload_project_finish (self, result, error);
}

void
ide_workbench_addin_workspace_added (IdeWorkbenchAddin *self,
                                     IdeWorkspace      *workspace)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->workspace_added)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->workspace_added (self, workspace);
}

void
ide_workbench_addin_workspace_removed (IdeWorkbenchAddin *self,
                                       IdeWorkspace      *workspace)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->workspace_removed)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->workspace_removed (self, workspace);
}

gboolean
ide_workbench_addin_can_open (IdeWorkbenchAddin *self,
                              GFile             *file,
                              const gchar       *content_type,
                              gint              *priority)
{
  gint real_priority;

  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (priority == NULL)
    priority = &real_priority;
  else
    *priority = 0;

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->can_open)
    return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->can_open (self, file, content_type, priority);

  return FALSE;
}

void
ide_workbench_addin_open_async (IdeWorkbenchAddin   *self,
                                GFile               *file,
                                const gchar         *content_type,
                                IdeBufferOpenFlags   flags,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_async (self,
                                                    file,
                                                    content_type,
                                                    flags,
                                                    cancellable,
                                                    callback,
                                                    user_data);
}

void
ide_workbench_addin_open_at_async (IdeWorkbenchAddin   *self,
                                   GFile               *file,
                                   const gchar         *content_type,
                                   gint                 at_line,
                                   gint                 at_line_offset,
                                   IdeBufferOpenFlags   flags,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_at_async (self,
                                                       file,
                                                       content_type,
                                                       at_line,
                                                       at_line_offset,
                                                       flags,
                                                       cancellable,
                                                       callback,
                                                       user_data);
}

gboolean
ide_workbench_addin_open_finish (IdeWorkbenchAddin  *self,
                                 GAsyncResult       *result,
                                 GError            **error)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_WORKBENCH_ADDIN_GET_IFACE (self)->open_finish (self, result, error);
}

/**
 * ide_workbench_addin_vcs_changed:
 * @self: a #IdeWorkbenchAddin
 * @vcs: (nullable): an #IdeVcs
 *
 * This function notifies an #IdeWorkbenchAddin that the version control
 * system has changed. This happens when ide_workbench_set_vcs() is called
 * or after an addin is loaded.
 *
 * This is helpful for plugins that want to react to VCS changes such as
 * changing branches, or tracking commits.
 *
 * Since: 3.32
 */
void
ide_workbench_addin_vcs_changed (IdeWorkbenchAddin *self,
                                 IdeVcs            *vcs)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_VCS (vcs));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->vcs_changed)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->vcs_changed (self, vcs);
}

/**
 * ide_workbench_addin_project_loaded:
 * @self: an #IdeWorkbenchAddin
 * @project_info: an #IdeProjectInfo
 *
 * This function is called after the project has been loaded.
 *
 * It is useful for situations where you do not need to influence the
 * project loading, but do need to perform operations after it has
 * completed.
 *
 * Since: 3.32
 */
void
ide_workbench_addin_project_loaded (IdeWorkbenchAddin *self,
                                    IdeProjectInfo    *project_info)
{
  g_return_if_fail (IDE_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  if (IDE_WORKBENCH_ADDIN_GET_IFACE (self)->project_loaded)
    IDE_WORKBENCH_ADDIN_GET_IFACE (self)->project_loaded (self, project_info);
}
