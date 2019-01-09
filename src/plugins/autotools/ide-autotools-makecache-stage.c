/* ide-autotools-makecache-stage.c
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

#define G_LOG_DOMAIN "ide-autotools-makecache-stage"

#include <glib/gi18n.h>

#include "ide-autotools-makecache-stage.h"

struct _IdeAutotoolsMakecacheStage
{
  IdeBuildStageLauncher  parent_instance;

  IdeMakecache          *makecache;
  IdeRuntime            *runtime;
  GFile                 *cache_file;
};

G_DEFINE_TYPE (IdeAutotoolsMakecacheStage, ide_autotools_makecache_stage, IDE_TYPE_BUILD_STAGE_LAUNCHER)

static void
ide_autotools_makecache_stage_makecache_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeMakecache) makecache = NULL;
  g_autoptr(GError) error = NULL;
  IdeAutotoolsMakecacheStage *self;

  IDE_ENTRY;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  makecache = ide_makecache_new_for_cache_file_finish (result, &error);

  if (makecache == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (self));

  ide_clear_and_destroy_object (&self->makecache);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (makecache));

  self->makecache = g_steal_pointer (&makecache);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_autotools_makecache_stage_execute_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeAutotoolsMakecacheStage *self = (IdeAutotoolsMakecacheStage *)object;
  IdeBuildStage *stage = (IdeBuildStage *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (stage));
  g_assert (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!IDE_BUILD_STAGE_CLASS (ide_autotools_makecache_stage_parent_class)->execute_finish (stage, result, &error))
    {
      g_warning ("%s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Now that we have our makecache file created, we can mmap() it into our
   * application address space using IdeMakecache.
   */

  ide_makecache_new_for_cache_file_async (self->runtime,
                                          self->cache_file,
                                          cancellable,
                                          ide_autotools_makecache_stage_makecache_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_autotools_makecache_stage_execute_async (IdeBuildStage       *stage,
                                             IdeBuildPipeline    *pipeline,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeAutotoolsMakecacheStage *self = (IdeAutotoolsMakecacheStage *)stage;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_makecache_stage_execute_async);

  /*
   * First we need to execute our launcher (performed by our parent class).
   * Only after that has succeeded to we move on to loading the makecache file
   * by mmap()'ing the generated make output.
   */

  IDE_BUILD_STAGE_CLASS (ide_autotools_makecache_stage_parent_class)->execute_async (stage,
                                                                                     pipeline,
                                                                                     cancellable,
                                                                                     ide_autotools_makecache_stage_execute_cb,
                                                                                     g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_makecache_stage_execute_finish (IdeBuildStage  *stage,
                                              GAsyncResult   *result,
                                              GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_autotools_makecache_stage_finalize (GObject *object)
{
  IdeAutotoolsMakecacheStage *self = (IdeAutotoolsMakecacheStage *)object;

  IDE_ENTRY;

  g_clear_object (&self->makecache);
  g_clear_object (&self->cache_file);
  g_clear_object (&self->runtime);

  G_OBJECT_CLASS (ide_autotools_makecache_stage_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
ide_autotools_makecache_stage_class_init (IdeAutotoolsMakecacheStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_autotools_makecache_stage_finalize;

  build_stage_class->execute_async = ide_autotools_makecache_stage_execute_async;
  build_stage_class->execute_finish = ide_autotools_makecache_stage_execute_finish;
}

static void
ide_autotools_makecache_stage_init (IdeAutotoolsMakecacheStage *self)
{
  ide_build_stage_set_name (IDE_BUILD_STAGE (self), _("Building cache…"));
  ide_build_stage_launcher_set_use_pty (IDE_BUILD_STAGE_LAUNCHER (self), FALSE);
}

IdeBuildStage *
ide_autotools_makecache_stage_new_for_pipeline (IdeBuildPipeline  *pipeline,
                                                GError           **error)
{
  g_autoptr(IdeAutotoolsMakecacheStage) stage = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autofree gchar *cache_path = NULL;
  const gchar *make = "make";
  IdeConfiguration *config;
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (pipeline), NULL);

  config = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_configuration_get_runtime (config);

  cache_path = ide_build_pipeline_build_builddir_path (pipeline, "Makecache", NULL);

  if (ide_runtime_contains_program_in_path (runtime, "gmake", NULL))
    make = "gmake";

  if (NULL == (launcher = ide_build_pipeline_create_launcher (pipeline, error)))
    IDE_RETURN (NULL);

  ide_subprocess_launcher_push_argv (launcher, make);
  ide_subprocess_launcher_push_argv (launcher, "-p");
  ide_subprocess_launcher_push_argv (launcher, "-n");
  ide_subprocess_launcher_push_argv (launcher, "-s");

  stage = g_object_new (IDE_TYPE_AUTOTOOLS_MAKECACHE_STAGE,
                        "launcher", launcher,
                        "ignore-exit-status", TRUE,
                        NULL);

  ide_build_stage_set_stdout_path (IDE_BUILD_STAGE (stage), cache_path);

  g_assert_cmpint (ide_build_stage_launcher_get_ignore_exit_status (IDE_BUILD_STAGE_LAUNCHER (stage)), ==, TRUE);

  stage->runtime = g_object_ref (runtime);
  stage->cache_file = g_file_new_for_path (cache_path);

  return IDE_BUILD_STAGE (g_steal_pointer (&stage));
}

IdeMakecache *
ide_autotools_makecache_stage_get_makecache (IdeAutotoolsMakecacheStage *self)
{
  g_return_val_if_fail (IDE_IS_AUTOTOOLS_MAKECACHE_STAGE (self), NULL);

  return self->makecache;
}
