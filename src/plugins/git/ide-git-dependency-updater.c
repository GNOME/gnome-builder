/* ide-git-dependency-updater.c
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

#define G_LOG_DOMAIN "ide-git-dependency-updater"

#include "config.h"

#include "ide-git-dependency-updater.h"
#include "ide-git-submodule-stage.h"

struct _IdeGitDependencyUpdater
{
  IdeObject parent_instance;
};

static void
find_submodule_stage_cb (gpointer data,
                         gpointer user_data)
{
  IdeGitSubmoduleStage **stage = user_data;

  g_assert (IDE_IS_BUILD_STAGE (data));
  g_assert (stage != NULL);
  g_assert (*stage == NULL || IDE_IS_BUILD_STAGE (*stage));

  if (IDE_IS_GIT_SUBMODULE_STAGE (data))
    *stage = IDE_GIT_SUBMODULE_STAGE (data);
}

static void
ide_git_dependency_updater_update_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeBuildManager *manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_manager_rebuild_finish (manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_git_dependency_updater_update_async (IdeDependencyUpdater *self,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeGitSubmoduleStage *stage = NULL;
  IdeBuildPipeline *pipeline;
  IdeBuildManager *manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_DEPENDENCY_UPDATER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_git_dependency_updater_update_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_build_manager (context);
  pipeline = ide_build_manager_get_pipeline (manager);

  g_assert (!pipeline || IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Cannot update git submodules until build pipeline is initialized");
      IDE_EXIT;
    }

  /* Find the submodule stage and tell it to download updates one time */
  ide_build_pipeline_foreach_stage (pipeline, find_submodule_stage_cb, &stage);

  if (stage == NULL)
    {
      /* Synthesize success if there is no submodule stage */
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  ide_git_submodule_stage_force_update (stage);

  /* Ensure downloads and everything past it is invalidated */
  ide_build_pipeline_invalidate_phase (pipeline, IDE_BUILD_PHASE_DOWNLOADS);

  /* Start building all the way up to the project configure so that
   * the user knows if the updates broke their configuration or anything.
   *
   * TODO: This should probably be done by the calling API so that we don't
   *       race with other updaters.
   */
  ide_build_manager_rebuild_async (manager,
                                   IDE_BUILD_PHASE_CONFIGURE,
                                   NULL,
				   ide_git_dependency_updater_update_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_git_dependency_updater_update_finish (IdeDependencyUpdater  *self,
                                          GAsyncResult          *result,
                                          GError               **error)
{
  g_assert (IDE_IS_GIT_DEPENDENCY_UPDATER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
dependency_updater_iface_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = ide_git_dependency_updater_update_async;
  iface->update_finish = ide_git_dependency_updater_update_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeGitDependencyUpdater, ide_git_dependency_updater, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DEPENDENCY_UPDATER,
                                                dependency_updater_iface_init))

static void
ide_git_dependency_updater_class_init (IdeGitDependencyUpdaterClass *klass)
{
}

static void
ide_git_dependency_updater_init (IdeGitDependencyUpdater *self)
{
}
