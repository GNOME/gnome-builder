/* ide-golang-go-stage.h
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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
 */

#define G_LOG_DOMAIN "ide-golang-go-stage"

#include "ide-golang-go-stage.h"

struct _IdeGolangGoStage
{
  IdeBuildStage  parent;

  /*
   * This is our primary build target. It will be run during the normal
   * execute_async()/execute_finish() pair.
   */
  gchar *target;

  /*
   * If we have a @clean_target, then we will run this make target during the
   * clean_async()/clean_finish() vfunc pair. They will not be run with
   * parallelism, because that just isn't very useful.
   */
  gchar *clean_target;
};

enum {
  PROP_0,
  PROP_TARGET,
  PROP_CLEAN_TARGET,
  N_PROPS
};

G_DEFINE_TYPE (IdeGolangGoStage, ide_golang_go_stage, IDE_TYPE_BUILD_STAGE)

static GParamSpec *properties [N_PROPS];

static IdeSubprocessLauncher *
create_launcher (IdeGolangGoStage  *self,
                 IdeBuildPipeline       *pipeline,
                 GCancellable           *cancellable,
                 const gchar            *go_target,
                 GError                **error)
{
  IdeConfiguration *config;
  IdeRuntime *runtime;
  const gchar *srcdir;
  const gchar *goroot;
  const gchar *gopath;

  g_autoptr(IdeSubprocessLauncher) launcher = NULL;

  g_assert (IDE_IS_GOLANG_GO_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (go_target != NULL);

  config = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_configuration_get_runtime (config);

  if (!ide_runtime_contains_program_in_path (runtime, "go", cancellable))
    {
      g_warning ("Unable to find 'go' program in path");
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unable to find 'go' program in path");
      return NULL;
    }

  if (NULL == (goroot = ide_configuration_getenv (config, "GOROOT")))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "GOROOT environment variable is not set");
      return NULL;
    }

  if (NULL == (gopath = ide_configuration_getenv (config, "GOPATH")))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "GOPATH environment variable is not set");
      return NULL;
    }

  srcdir = ide_build_pipeline_get_srcdir (pipeline);

  if (NULL == (launcher = ide_build_pipeline_create_launcher (pipeline, error)))
    return NULL;

  ide_subprocess_launcher_set_cwd (launcher, srcdir);
  ide_subprocess_launcher_set_flags (launcher,
                                     G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                     G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                     G_SUBPROCESS_FLAGS_STDERR_PIPE);

  ide_subprocess_launcher_push_argv (launcher, "go");
  ide_subprocess_launcher_push_argv (launcher, go_target);

  g_debug ("GOROOT is set to: %s", goroot);
  ide_subprocess_launcher_setenv (launcher, "GOROOT", goroot, TRUE);

  g_debug ("GOPATH is set to: %s", gopath);
  ide_subprocess_launcher_setenv (launcher, "GOPATH", gopath, TRUE);
  return g_steal_pointer (&launcher);
}

static void
ide_golang_go_stage_wait_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_golang_go_stage_execute_async (IdeBuildStage       *stage,
                                   IdeBuildPipeline    *pipeline,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  IdeGolangGoStage *self = (IdeGolangGoStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;
  const gchar *target;

  IDE_ENTRY;

  g_assert (IDE_IS_GOLANG_GO_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_golang_go_stage_execute_async);

  target = self->target;

  if (target == NULL)
    {
      g_warning ("Improperly configured IdeGolangGoStage, no target set");
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = create_launcher (self, pipeline, cancellable, target, &error);

  if (launcher == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Log the process arguments to stdout */
  argv = ide_subprocess_launcher_get_argv (launcher);
  message = g_strjoinv (" ", (gchar **)argv);
  ide_build_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_golang_go_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_golang_go_stage_execute_finish (IdeBuildStage  *stage,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_golang_go_stage_clean_async (IdeBuildStage       *stage,
                                 IdeBuildPipeline    *pipeline,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  IdeGolangGoStage *self = (IdeGolangGoStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;

  IDE_ENTRY;

  g_assert (IDE_IS_GOLANG_GO_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_golang_go_stage_clean_async);

  if (self->clean_target == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = create_launcher (self, pipeline, cancellable, self->clean_target, &error);

  if (launcher == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Log the process arguments to stdout */
  argv = ide_subprocess_launcher_get_argv (launcher);
  message = g_strjoinv (" ", (gchar **)argv);
  ide_build_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_golang_go_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_golang_go_stage_clean_finish (IdeBuildStage  *stage,
                                  GAsyncResult   *result,
                                  GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_golang_go_stage_query (IdeBuildStage    *stage,
                           IdeBuildPipeline *pipeline,
                           GCancellable     *cancellable)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GOLANG_GO_STAGE (stage));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* We always defer to make for completed state */
  ide_build_stage_set_completed (stage, FALSE);

  IDE_EXIT;
}

static void
ide_golang_go_stage_finalize (GObject *object)
{
  IdeGolangGoStage *self = (IdeGolangGoStage *)object;

  g_clear_pointer (&self->target, g_free);
  g_clear_pointer (&self->clean_target, g_free);

  G_OBJECT_CLASS (ide_golang_go_stage_parent_class)->finalize (object);
}

static void
ide_golang_go_stage_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeGolangGoStage *self = IDE_GOLANG_GO_STAGE (object);

  switch (prop_id)
    {
    case PROP_CLEAN_TARGET:
      g_value_set_string (value, self->clean_target);
      break;

    case PROP_TARGET:
      g_value_set_string (value, self->target);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_golang_go_stage_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeGolangGoStage *self = IDE_GOLANG_GO_STAGE (object);

  switch (prop_id)
    {
    case PROP_CLEAN_TARGET:
      g_free (self->clean_target);
      self->clean_target = g_value_dup_string (value);
      break;

    case PROP_TARGET:
      g_free (self->target);
      self->target = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_golang_go_stage_class_init (IdeGolangGoStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_golang_go_stage_finalize;
  object_class->get_property = ide_golang_go_stage_get_property;
  object_class->set_property = ide_golang_go_stage_set_property;

  build_stage_class->execute_async = ide_golang_go_stage_execute_async;
  build_stage_class->execute_finish = ide_golang_go_stage_execute_finish;
  build_stage_class->clean_async = ide_golang_go_stage_clean_async;
  build_stage_class->clean_finish = ide_golang_go_stage_clean_finish;
  build_stage_class->query = ide_golang_go_stage_query;

  properties [PROP_TARGET] =
    g_param_spec_string ("target",
                         "Target",
                         "A go target for normal execution",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLEAN_TARGET] =
    g_param_spec_string ("clean-target",
                         "Clean Target",
                         "A go clean target for normal execution",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_golang_go_stage_init (IdeGolangGoStage *self)
{
}
