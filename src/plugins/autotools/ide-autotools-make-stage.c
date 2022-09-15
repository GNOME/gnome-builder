/* ide-autotools-make-stage.c
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

#define G_LOG_DOMAIN "ide-autotools-make-stage"

#include "ide-autotools-make-stage.h"

struct _IdeAutotoolsMakeStage
{
  IdePipelineStage  parent;

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
   * build_async()/build_finish() pair.
   */
  gchar *target;

  /*
   * This is our chained build target. It is set if we found that we could
   * coallesce with the next build stage during pipeline execution.  It is
   * cleared during build_async() so that supplimental executions are
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

G_DEFINE_FINAL_TYPE (IdeAutotoolsMakeStage, ide_autotools_make_stage, IDE_TYPE_PIPELINE_STAGE)

static GParamSpec *properties [N_PROPS];

static IdeSubprocessLauncher *
create_launcher (IdeAutotoolsMakeStage  *self,
                 IdePipeline            *pipeline,
                 GCancellable           *cancellable,
                 const gchar            *make_target,
                 GError                **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (make_target != NULL);

  if (self->make == NULL)
    {
      IdeConfig *config = ide_pipeline_get_config (pipeline);
      IdeRuntime *runtime = ide_config_get_runtime (config);

      if (ide_runtime_contains_program_in_path (runtime, "gmake", cancellable))
        self->make = "gmake";
      else
        self->make = "make";
    }

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);

  ide_run_context_append_argv (run_context, self->make);

  /* Force disable previous V=1 that might be set by environment
   * variables from things like flatpak. We really don't want to
   * show verbose output here, its just too much.
   */
  ide_run_context_append_argv (run_context, "V=0");

  if (!g_str_equal (make_target, "clean"))
    {
      g_autofree gchar *parallel = NULL;

      if (self->parallel < 0)
        parallel = g_strdup_printf ("-j%u", g_get_num_processors () + 1);
      else if (self->parallel == 0)
        parallel = g_strdup_printf ("-j%u", g_get_num_processors ());
      else
        parallel = g_strdup_printf ("-j%u", self->parallel);

      ide_run_context_append_argv (run_context, parallel);
    }

  ide_run_context_append_argv (run_context, make_target);

  /*
   * When doing the "make all" target, we need to force LANG=C so that
   * we can parse the directory changes (Entering directory foo). Otherwise,
   * we can't really give users diagnostics that are in the proper directory.
   */
  if (ide_str_equal0 ("all", make_target))
    {
      ide_run_context_setenv (run_context, "LANG", "C.UTF-8");
      ide_run_context_setenv (run_context, "LC_ALL", "C.UTF-8");
      ide_run_context_setenv (run_context, "LC_MESSAGES", "C.UTF-8");
    }

  if ((launcher = ide_run_context_end (run_context, error)))
    ide_pipeline_attach_pty (pipeline, launcher);

  return g_steal_pointer (&launcher);
}

static void
ide_autotools_make_stage_wait_cb (GObject      *object,
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
ide_autotools_make_stage_build_async (IdePipelineStage    *stage,
                                      IdePipeline         *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;
  const gchar *target;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_make_stage_build_async);

  /* If we have a chained target, we just execute that instead */
  if (self->chained_target)
    target = self->chained_target;
  else
    target = self->target;

  if (target == NULL)
    {
      g_warning ("Improperly configured IdeAutotoolsMakeStage, no target set");
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  launcher = create_launcher (self, pipeline, cancellable, target, &error);

  if (launcher == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Clear the chained target so we do not run it again */
  g_clear_pointer (&self->chained_target, g_free);

  /* Log the process arguments to stdout */
  argv = ide_subprocess_launcher_get_argv (launcher);
  message = g_strjoinv (" ", (gchar **)argv);
  ide_pipeline_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_pipeline_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_make_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_build_finish (IdePipelineStage  *stage,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_autotools_make_stage_clean_async (IdePipelineStage    *stage,
                                      IdePipeline         *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *message = NULL;
  const gchar * const *argv;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_make_stage_clean_async);

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
  ide_pipeline_stage_log (stage, IDE_BUILD_LOG_STDOUT, message, -1);

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_pipeline_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_make_stage_wait_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_clean_finish (IdePipelineStage  *stage,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_autotools_make_stage_query (IdePipelineStage *stage,
                                IdePipeline      *pipeline,
                                GPtrArray        *targets,
                                GCancellable     *cancellable)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_AUTOTOOLS_MAKE_STAGE (stage));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* We always defer to make for completed state */
  ide_pipeline_stage_set_completed (stage, FALSE);

  IDE_EXIT;
}

static gboolean
ide_autotools_make_stage_chain (IdePipelineStage *stage,
                                IdePipelineStage *next)
{
  IdeAutotoolsMakeStage *self = (IdeAutotoolsMakeStage *)stage;

  g_assert (IDE_IS_AUTOTOOLS_MAKE_STAGE (self));
  g_assert (IDE_IS_PIPELINE_STAGE (next));

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
  IdePipelineStageClass *build_stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_autotools_make_stage_finalize;
  object_class->get_property = ide_autotools_make_stage_get_property;
  object_class->set_property = ide_autotools_make_stage_set_property;

  build_stage_class->build_async = ide_autotools_make_stage_build_async;
  build_stage_class->build_finish = ide_autotools_make_stage_build_finish;
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
