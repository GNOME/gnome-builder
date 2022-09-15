/* gbp-cargo-dependency-updater.c
 *
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-cargo-dependency-updater"

#include "config.h"

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-cargo-build-system.h"
#include "gbp-cargo-dependency-updater.h"

struct _GbpCargoDependencyUpdater
{
  IdeObject parent_instance;
};

static void
gbp_cargo_dependency_updater_wait_check_cb (GObject      *object,
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
gbp_cargo_dependency_updater_update_async (IdeDependencyUpdater *updater,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *cargo_toml = NULL;
  g_autofree char *cargo = NULL;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;
  const char *builddir;
  IdeConfig *config;

  IDE_ENTRY;

  g_assert (GBP_IS_CARGO_DEPENDENCY_UPDATER (updater));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (updater, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cargo_dependency_updater_update_async);

  context = ide_object_get_context (IDE_OBJECT (updater));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_CARGO_BUILD_SYSTEM (build_system))
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
                                 "Cannot update cargo dependencies. Build pipeline is not initialized.");
      IDE_EXIT;
    }

  config = ide_pipeline_get_config (pipeline);
  cargo = gbp_cargo_build_system_locate_cargo (GBP_CARGO_BUILD_SYSTEM (build_system), pipeline, config);
  cargo_toml = gbp_cargo_build_system_get_cargo_toml_path (GBP_CARGO_BUILD_SYSTEM (build_system));
  builddir = ide_pipeline_get_builddir (pipeline);

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_append_args (run_context, IDE_STRV_INIT (cargo, "update", "--manifest-path", cargo_toml));
  ide_run_context_setenv (run_context, "CARGO_TARGET_DIR", builddir);

  if (!(launcher = ide_run_context_end (run_context, &error)))
    IDE_GOTO (handle_error);

  ide_pipeline_attach_pty (pipeline, launcher);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL)))
    IDE_GOTO (handle_error);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   gbp_cargo_dependency_updater_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;

handle_error:
  ide_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

static gboolean
gbp_cargo_dependency_updater_update_finish (IdeDependencyUpdater  *updater,
                                            GAsyncResult          *result,
                                            GError               **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_CARGO_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
dependency_updater_iface_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = gbp_cargo_dependency_updater_update_async;
  iface->update_finish = gbp_cargo_dependency_updater_update_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCargoDependencyUpdater, gbp_cargo_dependency_updater, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_DEPENDENCY_UPDATER, dependency_updater_iface_init))
static void
gbp_cargo_dependency_updater_class_init (GbpCargoDependencyUpdaterClass *klass)
{
}

static void
gbp_cargo_dependency_updater_init (GbpCargoDependencyUpdater *self)
{
}
