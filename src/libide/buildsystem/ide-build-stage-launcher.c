/* ide-build-stage-launcher.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-stage-launcher"

#include "ide-debug.h"

#include "buildsystem/ide-build-log.h"
#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-stage-launcher.h"
#include "subprocess/ide-subprocess.h"

typedef struct
{
  IdeSubprocessLauncher *launcher;
  IdeSubprocessLauncher *clean_launcher;
  guint                  ignore_exit_status : 1;
  guint                  use_pty : 1;
} IdeBuildStageLauncherPrivate;

enum {
  PROP_0,
  PROP_CLEAN_LAUNCHER,
  PROP_USE_PTY,
  PROP_IGNORE_EXIT_STATUS,
  PROP_LAUNCHER,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildStageLauncher, ide_build_stage_launcher, IDE_TYPE_BUILD_STAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_build_stage_launcher_wait_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  IdeBuildStageLauncher *self = NULL;
  IdeBuildStageLauncherPrivate *priv;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint exit_status;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (self));

  priv = ide_build_stage_launcher_get_instance_private (self);

  IDE_TRACE_MSG ("  %s.ignore_exit_status=%u",
                 G_OBJECT_TYPE_NAME (self),
                 priv->ignore_exit_status);

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_subprocess_get_if_signaled (subprocess))
    {
      g_task_return_new_error (task,
                               G_SPAWN_ERROR,
                               G_SPAWN_ERROR_FAILED,
                               "The process was terminated by signal %d",
                               ide_subprocess_get_term_sig (subprocess));
      IDE_EXIT;
    }

  exit_status = ide_subprocess_get_exit_status (subprocess);

  if (priv->ignore_exit_status)
    IDE_GOTO (ignore_exit_failures);

  if (!g_spawn_check_exit_status (exit_status, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

ignore_exit_failures:
  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_build_stage_launcher_notify_completed_cb (GTask                 *task,
                                              GParamSpec            *pspec,
                                              IdeBuildStageLauncher *launcher)
{
  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (launcher));

  ide_build_stage_set_active (IDE_BUILD_STAGE (launcher), FALSE);
}

static void
ide_build_stage_launcher_run (IdeBuildStage         *stage,
                              IdeSubprocessLauncher *launcher,
                              IdeBuildPipeline      *pipeline,
                              GCancellable          *cancellable,
                              GAsyncReadyCallback    callback,
                              gpointer               user_data)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)stage;
  G_GNUC_UNUSED IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  GSubprocessFlags flags;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_stage_launcher_run);
  g_task_set_priority (task, G_PRIORITY_LOW);

  g_signal_connect (task,
                    "notify::completed",
                    G_CALLBACK (ide_build_stage_launcher_notify_completed_cb),
                    self);

  ide_build_stage_set_active (IDE_BUILD_STAGE (self), TRUE);

  if (launcher == NULL)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  if (priv->use_pty)
    {
      ide_build_pipeline_attach_pty (pipeline, launcher);
    }
  else
    {
      flags = ide_subprocess_launcher_get_flags (launcher);

      /* Disable flags we do not want set for build pipeline stuff */

      if (flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE)
        flags &= ~G_SUBPROCESS_FLAGS_STDERR_SILENCE;

      if (flags & G_SUBPROCESS_FLAGS_STDERR_MERGE)
        flags &= ~G_SUBPROCESS_FLAGS_STDERR_MERGE;

      if (flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT)
        flags &= ~G_SUBPROCESS_FLAGS_STDIN_INHERIT;

      /* Ensure we have access to stdin/stdout streams */

      flags |= G_SUBPROCESS_FLAGS_STDOUT_PIPE;
      flags |= G_SUBPROCESS_FLAGS_STDERR_PIPE;

      ide_subprocess_launcher_set_flags (launcher, flags);
    }

  /* Now launch the process */

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!priv->use_pty)
    ide_build_stage_log_subprocess (IDE_BUILD_STAGE (self), subprocess);

  IDE_TRACE_MSG ("Waiting for process %s to complete, %s exit status",
                 ide_subprocess_get_identifier (subprocess),
                 priv->ignore_exit_status ? "ignoring" : "checking");

  ide_subprocess_wait_async (subprocess,
                             cancellable,
                             ide_build_stage_launcher_wait_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_build_stage_launcher_execute_async (IdeBuildStage       *stage,
                                        IdeBuildPipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)stage;
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));

  ide_build_stage_launcher_run (stage, priv->launcher, pipeline, cancellable, callback, user_data);
}

static gboolean
ide_build_stage_launcher_execute_finish (IdeBuildStage  *stage,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (stage));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_stage_launcher_clean_async (IdeBuildStage       *stage,
                                      IdeBuildPipeline    *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)stage;
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));

  ide_build_stage_launcher_run (stage, priv->clean_launcher, pipeline, cancellable, callback, user_data);
}

static gboolean
ide_build_stage_launcher_clean_finish (IdeBuildStage  *stage,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (stage));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_stage_launcher_finalize (GObject *object)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_clear_object (&priv->launcher);
  g_clear_object (&priv->clean_launcher);

  G_OBJECT_CLASS (ide_build_stage_launcher_parent_class)->finalize (object);
}

static void
ide_build_stage_launcher_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_CLEAN_LAUNCHER:
      g_value_set_object (value, ide_build_stage_launcher_get_clean_launcher (self));
      break;

    case PROP_USE_PTY:
      g_value_set_boolean (value, ide_build_stage_launcher_get_use_pty (self));
      break;

    case PROP_IGNORE_EXIT_STATUS:
      g_value_set_boolean (value, ide_build_stage_launcher_get_ignore_exit_status (self));
      break;

    case PROP_LAUNCHER:
      g_value_set_object (value, ide_build_stage_launcher_get_launcher (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_launcher_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_CLEAN_LAUNCHER:
      ide_build_stage_launcher_set_clean_launcher (self, g_value_get_object (value));
      break;

    case PROP_USE_PTY:
      ide_build_stage_launcher_set_use_pty (self, g_value_get_boolean (value));
      break;

    case PROP_IGNORE_EXIT_STATUS:
      ide_build_stage_launcher_set_ignore_exit_status (self, g_value_get_boolean (value));
      break;

    case PROP_LAUNCHER:
      ide_build_stage_launcher_set_launcher (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_launcher_class_init (IdeBuildStageLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_build_stage_launcher_finalize;
  object_class->get_property = ide_build_stage_launcher_get_property;
  object_class->set_property = ide_build_stage_launcher_set_property;

  build_stage_class->execute_async = ide_build_stage_launcher_execute_async;
  build_stage_class->execute_finish = ide_build_stage_launcher_execute_finish;
  build_stage_class->clean_async = ide_build_stage_launcher_clean_async;
  build_stage_class->clean_finish = ide_build_stage_launcher_clean_finish;

  properties [PROP_CLEAN_LAUNCHER] =
    g_param_spec_object ("clean-launcher",
                         "Clean Launcher",
                         "The subprocess launcher for cleaning",
                         IDE_TYPE_SUBPROCESS_LAUNCHER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_PTY] =
    g_param_spec_boolean ("use-pty",
                          "Use Pty",
                          "If the subprocess should have a Pty attached",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IGNORE_EXIT_STATUS] =
    g_param_spec_boolean ("ignore-exit-status",
                          "Ignore Exit Status",
                          "If the exit status of the subprocess should be ignored",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAUNCHER] =
    g_param_spec_object ("launcher",
                         "Launcher",
                         "The subprocess launcher to execute",
                         IDE_TYPE_SUBPROCESS_LAUNCHER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_build_stage_launcher_init (IdeBuildStageLauncher *self)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  priv->use_pty = TRUE;
}

/**
 * ide_build_stage_launcher_get_launcher:
 *
 * Returns: (transfer none): An #IdeSubprocessLauncher
 */
IdeSubprocessLauncher *
ide_build_stage_launcher_get_launcher (IdeBuildStageLauncher *self)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self), NULL);

  return priv->launcher;
}

void
ide_build_stage_launcher_set_launcher (IdeBuildStageLauncher *self,
                                       IdeSubprocessLauncher *launcher)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));
  g_return_if_fail (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (g_set_object (&priv->launcher, launcher))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAUNCHER]);
}

/**
 * ide_build_stage_launcher_new:
 * @context: An #IdeContext
 * @launcher: (nullable): An #IdeSubprocessLauncher or %NULL
 *
 * Creates a new #IdeBuildStageLauncher that can be attached to an
 * #IdeBuildPipeline.
 *
 * Returns: (transfer full): An #IdeBuildStageLauncher
 */
IdeBuildStage *
ide_build_stage_launcher_new (IdeContext            *context,
                              IdeSubprocessLauncher *launcher)
{
  return g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                       "context", context,
                       "launcher", launcher,
                       NULL);
}

/**
 * ide_build_stage_launcher_get_ignore_exit_status:
 *
 * Gets the "ignore-exit-status" property.
 *
 * If set to %TRUE, a non-zero exit status from the subprocess will not cause
 * the build stage to fail.
 */
gboolean
ide_build_stage_launcher_get_ignore_exit_status (IdeBuildStageLauncher *self)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self), FALSE);

  return priv->ignore_exit_status;
}

/**
 * ide_build_stage_launcher_set_ignore_exit_status:
 *
 * Sets the "ignore-exit-status" property.
 *
 * If set to %TRUE, a non-zero exit status from the subprocess will not cause
 * the build stage to fail.
 */
void
ide_build_stage_launcher_set_ignore_exit_status (IdeBuildStageLauncher *self,
                                                 gboolean               ignore_exit_status)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));

  ignore_exit_status = !!ignore_exit_status;

  if (priv->ignore_exit_status != ignore_exit_status)
    {
      priv->ignore_exit_status = ignore_exit_status;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IGNORE_EXIT_STATUS]);
      IDE_EXIT;
    }

  IDE_EXIT;
}

void
ide_build_stage_launcher_set_clean_launcher (IdeBuildStageLauncher *self,
                                             IdeSubprocessLauncher *clean_launcher)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));
  g_return_if_fail (IDE_IS_SUBPROCESS_LAUNCHER (clean_launcher));

  if (g_set_object (&priv->clean_launcher, clean_launcher))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLEAN_LAUNCHER]);
}

/**
 * ide_build_stage_launcher_get_clean_launcher:
 *
 * Returns: (nullable) (transfer none): An #IdeSubprocessLauncher or %NULL.
 */
IdeSubprocessLauncher *
ide_build_stage_launcher_get_clean_launcher (IdeBuildStageLauncher *self)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self), NULL);

  return priv->clean_launcher;
}

gboolean
ide_build_stage_launcher_get_use_pty (IdeBuildStageLauncher *self)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self), FALSE);

  return priv->use_pty;
}

/**
 * ide_build_stage_launcher_set_use_pty:
 * @self: a #IdeBuildStageLauncher
 * @use_pty: If a Pty should be used
 *
 * If @use_pty is set to %TRUE, a Pty will be attached to the process.
 *
 * Since: 3.28
 */
void
ide_build_stage_launcher_set_use_pty (IdeBuildStageLauncher *self,
                                      gboolean               use_pty)
{
  IdeBuildStageLauncherPrivate *priv = ide_build_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self));

  use_pty = !!use_pty;

  if (use_pty != priv->use_pty)
    {
      priv->use_pty = use_pty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_PTY]);
    }
}
