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

#include "gbp-git-client.h"
#include "gbp-git-dependency-updater.h"
#include "gbp-git-submodule-stage.h"

struct _GbpGitDependencyUpdater
{
  IdeObject parent_instance;
};

static void
gbp_git_dependency_updater_update_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeBuildManager *manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeNotification *notif;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  notif = ide_task_get_task_data (task);

  g_assert (notif != NULL);
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (!ide_build_manager_rebuild_finish (manager, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      ide_notification_withdraw_in_seconds (notif, -1);
    }
  else
    {
      ide_task_return_boolean (task, TRUE);
      ide_notification_withdraw (notif);
    }

  IDE_EXIT;
}

static void
gbp_git_dependency_updater_update_async (IdeDependencyUpdater *self,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  GbpGitClient *client;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_DEPENDENCY_UPDATER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);
  notif = ide_notification_new ();

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_dependency_updater_update_async);
  ide_task_set_task_data (task, g_object_ref (notif), g_object_unref);

  gbp_git_client_update_submodules_async (client,
                                          notif,
                                          cancellable,
                                          gbp_git_dependency_updater_update_cb,
                                          g_steal_pointer (&task));

  ide_notification_attach (notif, IDE_OBJECT (context));

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

G_DEFINE_TYPE_WITH_CODE (GbpGitDependencyUpdater, gbp_git_dependency_updater, IDE_TYPE_OBJECT,
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
