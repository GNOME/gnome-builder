/* ide-autotools-build-target-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-build-target-provider"

#include "ide-autotools-build-system.h"
#include "ide-autotools-build-target-provider.h"
#include "ide-autotools-makecache-stage.h"
#include "ide-makecache.h"

struct _IdeAutotoolsBuildTargetProvider
{
  IdeObject parent_instance;
};

static void
find_makecache_from_stage (gpointer data,
                           gpointer user_data)
{
  IdeMakecache **makecache = user_data;
  IdePipelineStage *stage = data;

  if (*makecache != NULL)
    return;

  if (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (stage))
    *makecache = ide_autotools_makecache_stage_get_makecache (IDE_AUTOTOOLS_MAKECACHE_STAGE (stage));
}

static void
ide_autotools_build_target_provider_get_targets_cb (GObject      *object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
  IdeMakecache *makecache = (IdeMakecache *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAKECACHE (makecache));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ret = ide_makecache_get_build_targets_finish (makecache, result, &error);

  if (ret == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&ret), g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_autotools_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data)
{
  IdeAutotoolsBuildTargetProvider *self = (IdeAutotoolsBuildTargetProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) builddir_file = NULL;
  IdePipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdeMakecache *makecache = NULL;
  IdeContext *context;
  const gchar *builddir;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_build_target_provider_get_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not an autotools build system, ignoring");
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  builddir = ide_pipeline_get_builddir (pipeline);
  builddir_file = g_file_new_for_path (builddir);

  /*
   * Locate our makecache by finding the makecache stage (which should have
   * successfully executed by now) and get makecache object. Then we can
   * locate the build flags for the file (which makecache will translate
   * into the appropriate build target).
   */

  ide_pipeline_foreach_stage (pipeline, find_makecache_from_stage, &makecache);

  if (makecache == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Failed to locate makecache");
      IDE_EXIT;
    }

  ide_makecache_get_build_targets_async (makecache,
                                         builddir_file,
                                         cancellable,
                                         ide_autotools_build_target_provider_get_targets_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray *
ide_autotools_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                        GAsyncResult            *result,
                                                        GError                 **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (IDE_PTR_ARRAY_STEAL_FULL (&ret));
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = ide_autotools_build_target_provider_get_targets_async;
  iface->get_targets_finish = ide_autotools_build_target_provider_get_targets_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeAutotoolsBuildTargetProvider,
                         ide_autotools_build_target_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                build_target_provider_iface_init))

static void
ide_autotools_build_target_provider_class_init (IdeAutotoolsBuildTargetProviderClass *klass)
{
}

static void
ide_autotools_build_target_provider_init (IdeAutotoolsBuildTargetProvider *self)
{
}
