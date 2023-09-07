/* ide-pipeline-stage-command.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-pipeline-stage-command"

#include "config.h"

#include <libide-threading.h>

#include "ide-pipeline-private.h"
#include "ide-pipeline-stage-command.h"
#include "ide-run-command.h"
#include "ide-run-context.h"

typedef struct
{
  IdeRunCommand *build_command;
  IdeRunCommand *clean_command;
  char *stdout_path;
  guint ignore_exit_status : 1;
} IdePipelineStageCommandPrivate;

enum {
  PROP_0,
  PROP_BUILD_COMMAND,
  PROP_CLEAN_COMMAND,
  PROP_IGNORE_EXIT_STATUS,
  PROP_STDOUT_PATH,
  N_PROPS
};

enum {
  CREATE_RUN_CONTEXT,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdePipelineStageCommand, ide_pipeline_stage_command, IDE_TYPE_PIPELINE_STAGE)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_pipeline_stage_command_wait_check_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  IdePipelineStageCommandPrivate *priv;
  IdePipelineStageCommand *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  priv = ide_pipeline_stage_command_get_instance_private (self);

  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (self));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error) &&
      priv->ignore_exit_status == FALSE)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_pipeline_stage_command_build_async (IdePipelineStage    *stage,
                                        IdePipeline         *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdePipelineStageCommand *self = (IdePipelineStageCommand *)stage;
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_command_build_async);

  if (priv->build_command == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  g_signal_emit (self, signals[CREATE_RUN_CONTEXT], 0, priv->build_command, &run_context);

  if (run_context == NULL)
    run_context = ide_pipeline_create_run_context (pipeline, priv->build_command);

  if (priv->stdout_path == NULL)
    _ide_pipeline_attach_pty_to_run_context (pipeline, run_context);

  if (!(launcher = ide_run_context_end (run_context, &error)))
    IDE_GOTO (handle_error);

  if (priv->stdout_path != NULL)
    ide_subprocess_launcher_set_stdout_file_path (launcher, priv->stdout_path);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    IDE_GOTO (handle_error);

  ide_subprocess_send_signal_upon_cancel (subprocess, cancellable, SIGKILL);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_pipeline_stage_command_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;

handle_error:
  ide_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

static gboolean
ide_pipeline_stage_command_build_finish (IdePipelineStage  *stage,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_pipeline_stage_command_clean_async (IdePipelineStage    *stage,
                                        IdePipeline         *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdePipelineStageCommand *self = (IdePipelineStageCommand *)stage;
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_command_clean_async);

  if (priv->clean_command == NULL)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  g_signal_emit (self, signals[CREATE_RUN_CONTEXT], 0, priv->clean_command, &run_context);

  if (run_context == NULL)
    run_context = ide_pipeline_create_run_context (pipeline, priv->clean_command);

  _ide_pipeline_attach_pty_to_run_context (pipeline, run_context);

  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_send_signal_upon_cancel (subprocess, cancellable, SIGKILL);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_pipeline_stage_command_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_pipeline_stage_command_clean_finish (IdePipelineStage  *stage,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PIPELINE_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static char *
ide_pipeline_stage_command_repr (IdeObject *object)
{
  IdePipelineStageCommand *self = (IdePipelineStageCommand *)object;
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);
  GString *str;

  g_assert (IDE_IS_PIPELINE_STAGE_COMMAND (self));

  str = g_string_new (G_OBJECT_TYPE_NAME (self));

  g_string_append_printf (str, " completed=%s",
                          ide_pipeline_stage_get_completed (IDE_PIPELINE_STAGE (self)) ? "yes" : "no");

  if (priv->build_command != NULL)
    {
      const char * const *argv = ide_run_command_get_argv (priv->build_command);

      g_string_append (str, " build:");

      if (argv != NULL && argv[0] != NULL)
        g_string_append_printf (str, " [%s ...]", argv[0]);
    }

  if (priv->clean_command != NULL)
    {
      const char * const *argv = ide_run_command_get_argv (priv->clean_command);

      g_string_append (str, " clean:");

      if (argv != NULL && argv[0] != NULL)
        g_string_append_printf (str, " [%s ...]", argv[0]);
    }

  return g_string_free (str, FALSE);
}

static void
ide_pipeline_stage_command_finalize (GObject *object)
{
  IdePipelineStageCommand *self = (IdePipelineStageCommand *)object;
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  g_clear_object (&priv->build_command);
  g_clear_object (&priv->clean_command);
  g_clear_pointer (&priv->stdout_path, g_free);

  G_OBJECT_CLASS (ide_pipeline_stage_command_parent_class)->finalize (object);
}

static void
ide_pipeline_stage_command_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdePipelineStageCommand *self = IDE_PIPELINE_STAGE_COMMAND (object);
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BUILD_COMMAND:
      g_value_set_object (value, priv->build_command);
      break;

    case PROP_CLEAN_COMMAND:
      g_value_set_object (value, priv->clean_command);
      break;

    case PROP_IGNORE_EXIT_STATUS:
      g_value_set_boolean (value, priv->ignore_exit_status);
      break;

    case PROP_STDOUT_PATH:
      g_value_set_string (value, priv->stdout_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_command_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdePipelineStageCommand *self = IDE_PIPELINE_STAGE_COMMAND (object);
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BUILD_COMMAND:
      g_set_object (&priv->build_command, g_value_get_object (value));
      break;

    case PROP_CLEAN_COMMAND:
      g_set_object (&priv->clean_command, g_value_get_object (value));
      break;

    case PROP_IGNORE_EXIT_STATUS:
      ide_pipeline_stage_command_set_ignore_exit_status (self, g_value_get_boolean (value));
      break;

    case PROP_STDOUT_PATH:
      ide_pipeline_stage_command_set_stdout_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_command_class_init (IdePipelineStageCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdePipelineStageClass *pipeline_stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_command_finalize;
  object_class->get_property = ide_pipeline_stage_command_get_property;
  object_class->set_property = ide_pipeline_stage_command_set_property;

  i_object_class->repr = ide_pipeline_stage_command_repr;

  pipeline_stage_class->build_async = ide_pipeline_stage_command_build_async;
  pipeline_stage_class->build_finish = ide_pipeline_stage_command_build_finish;
  pipeline_stage_class->clean_async = ide_pipeline_stage_command_clean_async;
  pipeline_stage_class->clean_finish = ide_pipeline_stage_command_clean_finish;

  properties [PROP_BUILD_COMMAND] =
    g_param_spec_object ("build-command",
                         "Build Command",
                         "The build command to execute",
                         IDE_TYPE_RUN_COMMAND,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CLEAN_COMMAND] =
    g_param_spec_object ("clean-command",
                         "Clean Command",
                         "The clean command to execute",
                         IDE_TYPE_RUN_COMMAND,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IGNORE_EXIT_STATUS] =
    g_param_spec_boolean ("ignore-exit-status", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STDOUT_PATH] =
    g_param_spec_string ("stdout-path", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdePipelineStageCommand::create-run-context:
   * @self: a #IdePipelineStageCommand
   * @command: an #IdeRunCommand
   *
   * Sets up the #IdeRunContext which will be used to hoist in the
   * #IdeRunCommand. If no run context is provided, then the build pipeline
   * will be used.
   *
   * Returns: (transfer full): an #IdeRunContext
   */
  signals [CREATE_RUN_CONTEXT] =
    g_signal_new_class_handler ("create-run-context",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                g_signal_accumulator_first_wins, NULL,
                                NULL,
                                IDE_TYPE_RUN_CONTEXT,
                                1,
                                IDE_TYPE_RUN_COMMAND);
}

static void
ide_pipeline_stage_command_init (IdePipelineStageCommand *self)
{
}

IdePipelineStage *
ide_pipeline_stage_command_new (IdeRunCommand *build_command,
                                IdeRunCommand *clean_command)
{
  g_return_val_if_fail (!build_command || IDE_IS_RUN_COMMAND (build_command), NULL);
  g_return_val_if_fail (!clean_command || IDE_IS_RUN_COMMAND (clean_command), NULL);

  return g_object_new (IDE_TYPE_PIPELINE_STAGE_COMMAND,
                       "build-command", build_command,
                       "clean-command", clean_command,
                       NULL);
}

void
ide_pipeline_stage_command_set_build_command (IdePipelineStageCommand *self,
                                              IdeRunCommand           *build_command)
{
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_COMMAND (self));
  g_return_if_fail (!build_command || IDE_IS_RUN_COMMAND (build_command));

  if (g_set_object (&priv->build_command, build_command))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_COMMAND]);
}

void
ide_pipeline_stage_command_set_clean_command (IdePipelineStageCommand *self,
                                              IdeRunCommand           *clean_command)
{
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_COMMAND (self));
  g_return_if_fail (!clean_command || IDE_IS_RUN_COMMAND (clean_command));

  if (g_set_object (&priv->clean_command, clean_command))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLEAN_COMMAND]);
}

void
ide_pipeline_stage_command_set_stdout_path (IdePipelineStageCommand *self,
                                            const char              *stdout_path)
{
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_COMMAND (self));

  if (g_set_str (&priv->stdout_path, stdout_path))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STDOUT_PATH]);
}

void
ide_pipeline_stage_command_set_ignore_exit_status (IdePipelineStageCommand *self,
                                                   gboolean                 ignore_exit_status)
{
  IdePipelineStageCommandPrivate *priv = ide_pipeline_stage_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE_COMMAND (self));

  ignore_exit_status = !!ignore_exit_status;

  if (priv->ignore_exit_status != ignore_exit_status)
    {
      priv->ignore_exit_status = ignore_exit_status;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IGNORE_EXIT_STATUS]);
    }
}
