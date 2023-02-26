/*
 * gbp-swift-dependency-updater.c
 *
 * Copyright 2023 JCWasmx86 <JCWasmx86@t-online.de>
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

#define G_LOG_DOMAIN "gbp-swift-dependency-updater"

#include "config.h"

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-swift-build-system.h"
#include "gbp-swift-dependency-updater.h"

struct _GbpSwiftDependencyUpdater
{
  IdeObject parent_instance;
};

static void
gbp_swift_dependency_updater_wait_check_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_swift_dependency_updater_update_async (IdeDependencyUpdater *updater,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *project_dir = NULL;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_SWIFT_DEPENDENCY_UPDATER (updater));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (updater, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_swift_dependency_updater_update_async);

  context = ide_object_get_context (IDE_OBJECT (updater));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_SWIFT_BUILD_SYSTEM (build_system))
    {
      ide_task_return_unsupported_error (task);
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 "Cannot update swift dependencies. Build pipeline is not initialized.");
      IDE_EXIT;
    }

  project_dir = gbp_swift_build_system_get_project_dir (GBP_SWIFT_BUILD_SYSTEM (build_system));

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_append_args (run_context, IDE_STRV_INIT ("swift", "package", "resolve"));
  ide_run_context_set_cwd (run_context, project_dir);

  if (!(launcher = ide_run_context_end (run_context, &error)))
    IDE_GOTO (handle_error);

  ide_pipeline_attach_pty (pipeline, launcher);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL)))
    IDE_GOTO (handle_error);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   gbp_swift_dependency_updater_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;

handle_error:
  ide_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

static gboolean
gbp_swift_dependency_updater_update_finish (IdeDependencyUpdater  *updater,
                                             GAsyncResult          *result,
                                             GError               **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SWIFT_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
dependency_updater_iface_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = gbp_swift_dependency_updater_update_async;
  iface->update_finish = gbp_swift_dependency_updater_update_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSwiftDependencyUpdater, gbp_swift_dependency_updater, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_DEPENDENCY_UPDATER, dependency_updater_iface_init))
static void
gbp_swift_dependency_updater_class_init (GbpSwiftDependencyUpdaterClass *klass)
{
}

static void
gbp_swift_dependency_updater_init (GbpSwiftDependencyUpdater *self)
{
}
