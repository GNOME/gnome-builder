/* gbp-git-workbench-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-workbench-addin"

#include "config.h"

#include <libide-vcs.h>

#include "daemon/ipc-git-service.h"

#include "gbp-git-client.h"
#include "gbp-git-workbench-addin.h"
#include "gbp-git-vcs.h"

struct _GbpGitWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

static void
gbp_git_workbench_addin_load_project_new_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  GbpGitWorkbenchAddin *self;
  g_autoptr(IpcGitRepository) repository = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(repository = ipc_git_repository_proxy_new_finish (result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);

  if (self->workbench != NULL)
    {
      g_autoptr(GbpGitVcs) vcs = gbp_git_vcs_new (repository);

      ide_workbench_set_vcs (self->workbench, IDE_VCS (vcs));
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_git_workbench_addin_load_project_open_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IpcGitService *service = (IpcGitService *)object;
  g_autofree gchar *obj_path = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GDBusConnection *connection;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (service));

  if (!ipc_git_service_call_open_finish (service, &obj_path, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ipc_git_repository_proxy_new (connection,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  obj_path,
                                  ide_task_get_cancellable (task),
                                  gbp_git_workbench_addin_load_project_new_cb,
                                  g_object_ref (task));
}

static void
gbp_git_workbench_addin_load_project_discover_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IpcGitService *service = (IpcGitService *)object;
  g_autofree gchar *git_location = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IPC_IS_GIT_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_service_call_discover_finish (service, &git_location, result, &error))
    {
      g_debug ("Not a git repository: %s", error->message);
      ide_task_return_unsupported_error (task);
      return;
    }

  ipc_git_service_call_open (service,
                             git_location,
                             ide_task_get_cancellable (task),
                             gbp_git_workbench_addin_load_project_open_cb,
                             g_object_ref (task));
}

static void
gbp_git_workbench_addin_load_project_service_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IpcGitService) service = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeProjectInfo *project_info;
  GFile *directory;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(service = gbp_git_client_get_service_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  project_info = ide_task_get_task_data (task);
  directory = ide_project_info_get_directory (project_info);

  if (!g_file_is_native (directory))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Cannot initialize git, not a local file-system");
      return;
    }

  ipc_git_service_call_discover (service,
                                 g_file_peek_path (directory),
                                 ide_task_get_cancellable (task),
                                 gbp_git_workbench_addin_load_project_discover_cb,
                                 g_object_ref (task));
}

static void
gbp_git_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                            IdeProjectInfo      *project_info,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  GbpGitClient *client;
  IdeContext *context;

  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_workbench_addin_load_project_async);
  ide_task_set_task_data (task, g_object_ref (project_info), g_object_unref);

  context = ide_workbench_get_context (self->workbench);
  client = gbp_git_client_from_context (context);

  gbp_git_client_get_service_async (client,
                                    cancellable,
                                    gbp_git_workbench_addin_load_project_service_cb,
                                    g_steal_pointer (&task));
}

static gboolean
gbp_git_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                             GAsyncResult       *result,
                                             GError            **error)
{
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_git_workbench_addin_load (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;

  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_git_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;

  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_git_workbench_addin_load;
  iface->unload = gbp_git_workbench_addin_unload;
  iface->load_project_async = gbp_git_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_git_workbench_addin_load_project_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitWorkbenchAddin, gbp_git_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_git_workbench_addin_class_init (GbpGitWorkbenchAddinClass *klass)
{
}

static void
gbp_git_workbench_addin_init (GbpGitWorkbenchAddin *self)
{
}
