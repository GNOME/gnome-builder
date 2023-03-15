/* ide-pipeline-stage-launcher.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pipeline-stage-launcher"

#include "config.h"

#include <libide-threading.h>

#include "ide-subprocess-launcher-private.h"

#include "ide-build-log.h"
#include "ide-pipeline.h"
#include "ide-pipeline-stage-launcher.h"

typedef struct
{
  IdeSubprocessLauncher *launcher;
  IdeSubprocessLauncher *clean_launcher;
  guint                  ignore_exit_status : 1;
  guint                  use_pty : 1;
} IdePipelineStageLauncherPrivate;

enum {
  PROP_0,
  PROP_CLEAN_LAUNCHER,
  PROP_USE_PTY,
  PROP_IGNORE_EXIT_STATUS,
  PROP_LAUNCHER,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdePipelineStageLauncher, ide_pipeline_stage_launcher, IDE_TYPE_PIPELINE_STAGE)

static GParamSpec *properties [N_PROPS];

static inline gboolean
needs_quoting (const gchar *str)
{
  for (; *str; str = g_utf8_next_char (str))
    {
      gunichar ch = g_utf8_get_char (str);

      switch (ch)
        {
        case '\'':
        case '"':
        case '\\':
          return TRUE;

        default:
          if (g_unichar_isspace (ch))
            return TRUE;
          break;
        }
    }

  return FALSE;
}

static gchar *
pretty_print_args (IdeSubprocessLauncher *launcher)
{
  const gchar * const *argv;
  g_autoptr(GString) command = NULL;

  g_assert (IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (!(argv = ide_subprocess_launcher_get_argv (launcher)))
    return NULL;

  command = g_string_new (NULL);

  for (guint i = 0; argv[i] != NULL; i++)
    {
      if (command->len > 0)
        g_string_append_c (command, ' ');

      if (needs_quoting (argv[i]))
        {
          g_autofree gchar *quoted = g_shell_quote (argv[i]);
          g_string_append (command, quoted);
        }
      else
        g_string_append (command, argv[i]);
    }

  return g_string_free (g_steal_pointer (&command), FALSE);
}

static void
ide_pipeline_stage_launcher_wait_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  IdePipelineStageLauncher *self = NULL;
  IdePipelineStageLauncherPrivate *priv;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gint exit_status;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

  priv = ide_pipeline_stage_launcher_get_instance_private (self);

  IDE_TRACE_MSG ("  %s.ignore_exit_status=%u",
                 G_OBJECT_TYPE_NAME (self),
                 priv->ignore_exit_status);

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_subprocess_get_if_signaled (subprocess))
    {
      ide_task_return_new_error (task,
                                 G_SPAWN_ERROR,
                                 G_SPAWN_ERROR_FAILED,
                                 "The process was terminated by signal %d",
                                 ide_subprocess_get_term_sig (subprocess));
      IDE_EXIT;
    }

  exit_status = ide_subprocess_get_exit_status (subprocess);

  if (priv->ignore_exit_status)
    IDE_GOTO (ignore_exit_failures);

  if (!g_spawn_check_wait_status (exit_status, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

ignore_exit_failures:
  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_pipeline_stage_launcher_notify_completed_cb (IdeTask                  *task,
                                                 GParamSpec               *pspec,
                                                 IdePipelineStageLauncher *launcher)
{
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (launcher));

  ide_pipeline_stage_set_active (IDE_PIPELINE_STAGE (launcher), FALSE);
}

static void
ide_pipeline_stage_launcher_run (IdePipelineStage      *stage,
                                 IdeSubprocessLauncher *launcher,
                                 IdePipeline           *pipeline,
                                 GCancellable          *cancellable,
                                 GAsyncReadyCallback    callback,
                                 gpointer               user_data)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)stage;
  G_GNUC_UNUSED IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  GSubprocessFlags flags;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_launcher_run);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  g_signal_connect (task,
                    "notify::completed",
                    G_CALLBACK (ide_pipeline_stage_launcher_notify_completed_cb),
                    self);

  ide_pipeline_stage_set_active (IDE_PIPELINE_STAGE (self), TRUE);

  if (launcher == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  if (priv->use_pty)
    {
      ide_pipeline_attach_pty (pipeline, launcher);
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

      if (!ide_subprocess_launcher_get_stdout_file_path (launcher))
        flags |= G_SUBPROCESS_FLAGS_STDOUT_PIPE;

      flags |= G_SUBPROCESS_FLAGS_STDERR_PIPE;

      ide_subprocess_launcher_set_flags (launcher, flags);
    }

  if (priv->use_pty)
    {
      g_autofree gchar *command = pretty_print_args (launcher);

      if (command != NULL)
        ide_pipeline_stage_log (IDE_PIPELINE_STAGE (self), IDE_BUILD_LOG_STDOUT, command, -1);
    }

  /* Now launch the process */

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!priv->use_pty)
    ide_pipeline_stage_log_subprocess (IDE_PIPELINE_STAGE (self), subprocess);

  IDE_TRACE_MSG ("Waiting for process %s to complete, %s exit status",
                 ide_subprocess_get_identifier (subprocess),
                 priv->ignore_exit_status ? "ignoring" : "checking");

  ide_subprocess_wait_async (subprocess,
                             cancellable,
                             ide_pipeline_stage_launcher_wait_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static void
ide_pipeline_stage_launcher_build_async (IdePipelineStage    *stage,
                                         IdePipeline         *pipeline,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)stage;
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

  ide_pipeline_stage_launcher_run (stage, priv->launcher, pipeline, cancellable, callback, user_data);
}

static gboolean
ide_pipeline_stage_launcher_build_finish (IdePipelineStage  *stage,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_pipeline_stage_launcher_clean_async (IdePipelineStage    *stage,
                                         IdePipeline         *pipeline,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)stage;
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

  ide_pipeline_stage_launcher_run (stage, priv->clean_launcher, pipeline, cancellable, callback, user_data);
}

static gboolean
ide_pipeline_stage_launcher_clean_finish (IdePipelineStage  *stage,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gchar *
ide_pipeline_stage_launcher_repr (IdeObject *object)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)object;
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);
  const gchar * const *argv = NULL;

  g_assert (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

  if (priv->launcher)
    argv = ide_subprocess_launcher_get_argv (priv->launcher);

  return g_strdup_printf ("%s [%s ...] use_pty=%d ignore_exit_status=%d",
                          G_OBJECT_TYPE_NAME (self),
                          argv && argv[0] ? argv[0] : "(unspecified)",
                          priv->use_pty,
                          priv->ignore_exit_status);
}

static void
ide_pipeline_stage_launcher_finalize (GObject *object)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)object;
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_clear_object (&priv->launcher);
  g_clear_object (&priv->clean_launcher);

  G_OBJECT_CLASS (ide_pipeline_stage_launcher_parent_class)->finalize (object);
}

static void
ide_pipeline_stage_launcher_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_CLEAN_LAUNCHER:
      g_value_set_object (value, ide_pipeline_stage_launcher_get_clean_launcher (self));
      break;

    case PROP_USE_PTY:
      g_value_set_boolean (value, ide_pipeline_stage_launcher_get_use_pty (self));
      break;

    case PROP_IGNORE_EXIT_STATUS:
      g_value_set_boolean (value, ide_pipeline_stage_launcher_get_ignore_exit_status (self));
      break;

    case PROP_LAUNCHER:
      g_value_set_object (value, ide_pipeline_stage_launcher_get_launcher (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_launcher_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdePipelineStageLauncher *self = (IdePipelineStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_CLEAN_LAUNCHER:
      ide_pipeline_stage_launcher_set_clean_launcher (self, g_value_get_object (value));
      break;

    case PROP_USE_PTY:
      ide_pipeline_stage_launcher_set_use_pty (self, g_value_get_boolean (value));
      break;

    case PROP_IGNORE_EXIT_STATUS:
      ide_pipeline_stage_launcher_set_ignore_exit_status (self, g_value_get_boolean (value));
      break;

    case PROP_LAUNCHER:
      ide_pipeline_stage_launcher_set_launcher (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_launcher_class_init (IdePipelineStageLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdePipelineStageClass *build_stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_launcher_finalize;
  object_class->get_property = ide_pipeline_stage_launcher_get_property;
  object_class->set_property = ide_pipeline_stage_launcher_set_property;

  i_object_class->repr = ide_pipeline_stage_launcher_repr;

  build_stage_class->build_async = ide_pipeline_stage_launcher_build_async;
  build_stage_class->build_finish = ide_pipeline_stage_launcher_build_finish;
  build_stage_class->clean_async = ide_pipeline_stage_launcher_clean_async;
  build_stage_class->clean_finish = ide_pipeline_stage_launcher_clean_finish;

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
                         "The subprocess launcher to build",
                         IDE_TYPE_SUBPROCESS_LAUNCHER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_pipeline_stage_launcher_init (IdePipelineStageLauncher *self)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  priv->use_pty = TRUE;
}

/**
 * ide_pipeline_stage_launcher_get_launcher:
 *
 * Returns: (transfer none): An #IdeSubprocessLauncher
 */
IdeSubprocessLauncher *
ide_pipeline_stage_launcher_get_launcher (IdePipelineStageLauncher *self)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self), NULL);

  return priv->launcher;
}

void
ide_pipeline_stage_launcher_set_launcher (IdePipelineStageLauncher *self,
                                          IdeSubprocessLauncher    *launcher)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));
  g_return_if_fail (!launcher || IDE_IS_SUBPROCESS_LAUNCHER (launcher));

  if (g_set_object (&priv->launcher, launcher))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAUNCHER]);
}

/**
 * ide_pipeline_stage_launcher_new:
 * @context: An #IdeContext
 * @launcher: (nullable): An #IdeSubprocessLauncher or %NULL
 *
 * Creates a new #IdePipelineStageLauncher that can be attached to an
 * #IdePipeline.
 *
 * Returns: (transfer full): An #IdePipelineStageLauncher
 */
IdePipelineStage *
ide_pipeline_stage_launcher_new (IdeContext            *context,
                                 IdeSubprocessLauncher *launcher)
{
  return g_object_new (IDE_TYPE_PIPELINE_STAGE_LAUNCHER,
                       "launcher", launcher,
                       NULL);
}

/**
 * ide_pipeline_stage_launcher_get_ignore_exit_status:
 *
 * Gets the "ignore-exit-status" property.
 *
 * If set to %TRUE, a non-zero exit status from the subprocess will not cause
 * the build stage to fail.
 */
gboolean
ide_pipeline_stage_launcher_get_ignore_exit_status (IdePipelineStageLauncher *self)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self), FALSE);

  return priv->ignore_exit_status;
}

/**
 * ide_pipeline_stage_launcher_set_ignore_exit_status:
 *
 * Sets the "ignore-exit-status" property.
 *
 * If set to %TRUE, a non-zero exit status from the subprocess will not cause
 * the build stage to fail.
 */
void
ide_pipeline_stage_launcher_set_ignore_exit_status (IdePipelineStageLauncher *self,
                                                    gboolean                  ignore_exit_status)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

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
ide_pipeline_stage_launcher_set_clean_launcher (IdePipelineStageLauncher *self,
                                                IdeSubprocessLauncher    *clean_launcher)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));
  g_return_if_fail (!clean_launcher || IDE_IS_SUBPROCESS_LAUNCHER (clean_launcher));

  if (g_set_object (&priv->clean_launcher, clean_launcher))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLEAN_LAUNCHER]);
}

/**
 * ide_pipeline_stage_launcher_get_clean_launcher:
 *
 * Returns: (nullable) (transfer none): An #IdeSubprocessLauncher or %NULL.
 */
IdeSubprocessLauncher *
ide_pipeline_stage_launcher_get_clean_launcher (IdePipelineStageLauncher *self)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self), NULL);

  return priv->clean_launcher;
}

gboolean
ide_pipeline_stage_launcher_get_use_pty (IdePipelineStageLauncher *self)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self), FALSE);

  return priv->use_pty;
}

/**
 * ide_pipeline_stage_launcher_set_use_pty:
 * @self: a #IdePipelineStageLauncher
 * @use_pty: If a Pty should be used
 *
 * If @use_pty is set to %TRUE, a Pty will be attached to the process.
 */
void
ide_pipeline_stage_launcher_set_use_pty (IdePipelineStageLauncher *self,
                                         gboolean                  use_pty)
{
  IdePipelineStageLauncherPrivate *priv = ide_pipeline_stage_launcher_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_LAUNCHER (self));

  use_pty = !!use_pty;

  if (use_pty != priv->use_pty)
    {
      priv->use_pty = use_pty;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_PTY]);
    }
}
