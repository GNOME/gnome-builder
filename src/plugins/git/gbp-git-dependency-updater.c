/* gbp-git-dependency-updater.c
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

#define G_LOG_DOMAIN "gbp-git-dependency-updater"

#include "config.h"

#include <glib/gi18n.h>

#include "daemon/ipc-git-repository.h"

#include "gbp-git-dependency-updater.h"
#include "gbp-git-progress.h"
#include "gbp-git-vcs.h"

struct _GbpGitDependencyUpdater
{
  IdeObject parent_instance;
};

static void
gbp_git_dependency_updater_update_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IpcGitRepository *repository = (IpcGitRepository *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IPC_IS_GIT_REPOSITORY (repository));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ipc_git_repository_call_update_submodules_finish (repository, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_git_dependency_updater_update_async (IdeDependencyUpdater *self,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_autoptr(IpcGitProgress) progress = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GError) error = NULL;
  IpcGitRepository *repository;
  GDBusConnection *connection;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_DEPENDENCY_UPDATER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_dependency_updater_update_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_VCS);

  if (!GBP_IS_GIT_VCS (vcs))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 _("Git version control is not in use"));
      IDE_EXIT;
    }

  repository = gbp_git_vcs_get_repository (GBP_GIT_VCS (vcs));
  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (repository));

  notif = g_object_new (IDE_TYPE_NOTIFICATION,
                        "title", _("Updating Git Submodules"),
                        NULL);

  if (!(progress = gbp_git_progress_new (connection, notif, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_task_set_task_data (task, g_object_ref (progress), g_object_unref);
  gbp_git_progress_set_withdraw (GBP_GIT_PROGRESS (progress), TRUE);
  ide_notification_attach (notif, IDE_OBJECT (context));

  ipc_git_repository_call_update_submodules (repository,
                                             TRUE,
                                             g_dbus_interface_skeleton_get_object_path (G_DBUS_INTERFACE_SKELETON (progress)),
                                             cancellable,
                                             gbp_git_dependency_updater_update_cb,
                                             g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_git_dependency_updater_update_finish (IdeDependencyUpdater  *self,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  g_assert (GBP_IS_GIT_DEPENDENCY_UPDATER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
dependency_updater_iface_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = gbp_git_dependency_updater_update_async;
  iface->update_finish = gbp_git_dependency_updater_update_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitDependencyUpdater, gbp_git_dependency_updater, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DEPENDENCY_UPDATER,
                                                dependency_updater_iface_init))

static void
gbp_git_dependency_updater_class_init (GbpGitDependencyUpdaterClass *klass)
{
}

static void
gbp_git_dependency_updater_init (GbpGitDependencyUpdater *self)
{
}
