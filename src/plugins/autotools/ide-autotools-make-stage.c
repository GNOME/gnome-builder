/* ide-autotools-make-stage.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-make-stage"

#include "ide-autotools-make-stage.h"

struct _IdeAutotoolsMakeStage
{
  IdeBuildStage  parent;

  /*
   * If we discover "gmake", then this will be "gmake". If it is NULL then we
   * have not yet discovered if "gmake" is available. If there is no "gmake",
   * and we have checked, this will be "make".
   *
   * We have to do this because we might be on a system where "gmake" is not
   * available (say inside of flatpak), and some systems such as FreeBSD
   * require "gmake" because "make" is not very GNU compatible.
   */
  const gchar *make;

  /*
   * This is our primary build target. It will be run during the normal
   * execute_async()/execute_finish() pair.
   */
  gchar *target;

  /*
   * This is our chained build target. It is set if we found that we could
   * coallesce with the next build stage during pipeline execution.  It is
   * cleared during execute_async() so that supplimental executions are
   * unaffected.
   */
  gchar *chained_target;

  /*
   * If we have a @clean_target, then we will run this make target during the
   * clean_async()/clean_finish() vfunc pair. They will not be run with
   * parallelism, because that just isn't very useful.
   */
  gchar *clean_target;

  /*
   * If we should perform parallel builds with "make -jN".
   */
  gint parallel;
};

enum {
  PROP_0,
  PROP_CLEAN_TARGET,
  PROP_PARALLEL,
  PROP_TARGET,
  N_PROPS
};

G_DEFINE_TYPE (IdeAutotoolsMakeStage, ide_autotools_make_stage, IDE_TYPE_BUILD_STAGE)

static GParamSpec *properties [N_PROPS];

static IdeSubprocessLauncher *
create_launcher (IdeAutotoolsMakeStage  *self,
                 IdeBuildPipeline       *pipeline,
                 GCancellable           *cancellable,
                 const gchar            *make_target,
                 GError                **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (make_target != NULL);

  if (self->make == NULL)
    {
      IdeConfiguration *config = ide_build_pipeline_get_configuration (pipeline);
      IdeRuntime *runtime = ide_configuration_get_runtime (config);

      if (ide_runtime_contains_program_in_path (runtime, "gmake", cancellable))
        self->make = "gmake";
      else
        self->make = "make";
    }

  if (NULL == (launcher = ide_build_pipeline_create_launcher (pipeline, error)))
    return NULL;

  ide_subprocess_launcher_set_flags (launcher,
                                     G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                     G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                     G_SUBPROCESS_FLAGS_STDERR_PIPE);

  ide_subprocess_launcher_push_argv (launcher, self->make);

  /* Force disable previous V=1 that might be set by environment
   * variables from things like flatpak. We really don't want to
   * show verbose output here, its just too much.
   */
  ide_subprocess_launcher_push_argv (launcher, "V=0");

  if (!g_str_equal (make_target, "clean"))
    {
      g_autofree gchar *parallel = NULL;

      if (self->parallel < 0)
        parallel = g_strdup_printf ("-j%u", g_get_num_processors () + 1);
      else if (self->parallel == 0)
        parallel = g_strdup_printf ("-j%u", g_get_num_processors ());
      else
        parallel = g_strdup_printf ("-j%u", self->parallel);

      ide_subprocess_launcher_push_argv (launcher, parallel);
    }

  ide_subprocess_launcher_push_argv (launcher, make_target);

  return g_steal_pointer (&launcher);
}

static void
ide_autotools_make_stage_wait_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_autotools_make_stage_execute_async (IdeBuildStage       *stage,
                                        IdeBuildPipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;
  const gchar *target;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_make_stage_execute_async);

  /* If we have a chained target, we just execute that instead */
  if (self->chained_target)
    target = self->chained_target;
  else
    target = self->target;

  if (target == NULL)
    {
      g_warning ("Improperly configured IdeAutotoolsMakeStage, no target set");
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = create_launcher (self, pipeline, cancellable, target, &error);

  if (launcher == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Clear the chained target so we do not run it again */
  g_clear_pointer (&self->chained_target, g_free);

  /* Log the process arguments to stdout */
  argv = ide_subprocess_launcher_get_argv (launcher);
  message = g_strjoinv (" ", (gchar **)argv);
  ide_build_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_make_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_execute_finish (IdeBuildStage  *stage,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_autotools_make_stage_clean_async (IdeBuildStage       *stage,
                                      IdeBuildPipeline    *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_make_stage_clean_async);

  if (self->clean_target == NULL)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = create_launcher (self, pipeline, cancellable, self->clean_target, &error);

  if (launcher == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Log the process arguments to stdout */
  argv = ide_subprocess_launcher_get_argv (launcher);
  message = g_strjoinv (" ", (gchar **)argv);
  ide_build_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_make_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_clean_finish (IdeBuildStage  *stage,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE (stage));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_autotools_make_stage_query (IdeBuildStage    *stage,
                                IdeBuildPipeline *pipeline,
                                GCancellable     *cancellable)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_AUTOTOOLS_MAKE_STAGE (stage));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* We always defer to make for completed state */
  ide_build_stage_set_completed (stage, FALSE);

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_chain (IdeBuildStage *stage,
                                IdeBuildStage *next)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_BUILD_STAGE (next));

  if (IDE_IS_AUTOTOOLS_MAKE_STAGE (next))
    {
      /* If this is a `make all` and the next is `make install`, we can skip
       * the `make all` target and do them as a single `make install` step.
       */
      if ((g_strcmp0 (self->target, "all") == 0) &&
          (g_strcmp0 (IDE_AUTOTOOLS_MAKE_STAGE (next)->target, "install") == 0))
      {
        g_clear_pointer (&self->chained_target, g_free);
        self->chained_target = g_strdup ("install");
        return TRUE;
      }
    }

  return FALSE;
}

static void
ide_autotools_make_stage_finalize (GObject *object)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)object;

  g_clear_pointer (&self->target, g_free);
  g_clear_pointer (&self->chained_target, g_free);
  g_clear_pointer (&self->clean_target, g_free);

  G_OBJECT_CLASS (ide_autotools_make_stage_parent_class)->finalize (object);
}

static void
ide_autotools_make_stage_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeAutotoolsMakeStage *self = IDE_AUTOTOOLS_MAKE_STAGE (object);

  switch (prop_id)
    {
    case PROP_CLEAN_TARGET:
      g_value_set_string (value, self->clean_target);
      break;

    case PROP_PARALLEL:
      g_value_set_int (value, self->parallel);
      break;

    case PROP_TARGET:
      g_value_set_string (value, self->target);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_make_stage_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeAutotoolsMakeStage *self = IDE_AUTOTOOLS_MAKE_STAGE (object);

  switch (prop_id)
    {
    case PROP_CLEAN_TARGET:
      g_free (self->clean_target);
      self->clean_target = g_value_dup_string (value);
      break;

    case PROP_PARALLEL:
      self->parallel = g_value_get_int (value);
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
ide_autotools_make_stage_class_init (IdeAutotoolsMakeStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_autotools_make_stage_finalize;
  object_class->get_property = ide_autotools_make_stage_get_property;
  object_class->set_property = ide_autotools_make_stage_set_property;

  build_stage_class->execute_async = ide_autotools_make_stage_execute_async;
  build_stage_class->execute_finish = ide_autotools_make_stage_execute_finish;
  build_stage_class->clean_async = ide_autotools_make_stage_clean_async;
  build_stage_class->clean_finish = ide_autotools_make_stage_clean_finish;
  build_stage_class->query = ide_autotools_make_stage_query;
  build_stage_class->chain = ide_autotools_make_stage_chain;

  properties [PROP_CLEAN_TARGET] =
    g_param_spec_string ("clean-target",
                         "Clean Target",
                         "A make target to execute for clean operations",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TARGET] =
    g_param_spec_string ("target",
                         "Target",
                         "A make target for normal execution",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARALLEL] =
    g_param_spec_int ("parallel",
                      "Parallel",
                      "The amount of parellelism to use",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_autotools_make_stage_init (IdeAutotoolsMakeStage *self)
{
  self->parallel = -1;
}
