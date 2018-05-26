/* ide-build-stage.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-stage"

#include "config.h"

#include <string.h>

#include "ide-debug.h"

#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-stage.h"
#include "subprocess/ide-subprocess.h"
#include "threading/ide-task.h"

typedef struct
{
  gchar               *name;
  IdeBuildLogObserver  observer;
  gpointer             observer_data;
  GDestroyNotify       observer_data_destroy;
  IdeTask             *queued_execute;
  gchar               *stdout_path;
  GOutputStream       *stdout_stream;
  gint                 n_pause;
  IdeBuildPhase        phase;
  guint                completed : 1;
  guint                disabled : 1;
  guint                transient : 1;
  guint                check_stdout : 1;
  guint                active : 1;
} IdeBuildStagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildStage, ide_build_stage, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_CHECK_STDOUT,
  PROP_COMPLETED,
  PROP_DISABLED,
  PROP_NAME,
  PROP_STDOUT_PATH,
  PROP_TRANSIENT,
  N_PROPS
};

enum {
  CHAIN,
  QUERY,
  REAP,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

typedef struct
{
  IdeBuildStage     *self;
  GOutputStream     *stream;
  IdeBuildLogStream  stream_type;
} Tail;

static Tail *
tail_new (IdeBuildStage     *self,
          GOutputStream     *stream,
          IdeBuildLogStream  stream_type)
{
  Tail *tail;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (!stream || G_IS_OUTPUT_STREAM (stream));
  g_assert (stream_type == IDE_BUILD_LOG_STDOUT || stream_type == IDE_BUILD_LOG_STDERR);

  tail = g_slice_new0 (Tail);
  tail->self = g_object_ref (self);
  tail->stream = stream ? g_object_ref (stream) : NULL;
  tail->stream_type = stream_type;

  return tail;
}

static void
tail_free (Tail *tail)
{
  IDE_ENTRY;

  g_clear_object (&tail->self);
  g_clear_object (&tail->stream);
  g_slice_free (Tail, tail);

  IDE_EXIT;
}

static void
ide_build_stage_clear_observer (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);
  GDestroyNotify notify = priv->observer_data_destroy;
  gpointer data = priv->observer_data;

  priv->observer_data_destroy = NULL;
  priv->observer_data = NULL;
  priv->observer = NULL;

  if (notify != NULL)
    notify (data);
}

static gboolean
ide_build_stage_real_execute (IdeBuildStage     *self,
                              IdeBuildPipeline  *pipeline,
                              GCancellable      *cancellable,
                              GError           **error)
{
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  return TRUE;
}

static void
ide_build_stage_real_execute_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeBuildStage *self = source_object;
  IdeBuildPipeline *pipeline = task_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (IDE_BUILD_STAGE_GET_CLASS (self)->execute (self, pipeline, cancellable, &error))
    ide_task_return_boolean (task, TRUE);
  else
    ide_task_return_error (task, g_steal_pointer (&error));
}

static void
ide_build_stage_real_execute_async (IdeBuildStage       *self,
                                    IdeBuildPipeline    *pipeline,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_stage_real_execute_async);
  ide_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);
  ide_task_run_in_thread (task, ide_build_stage_real_execute_worker);
}

static gboolean
ide_build_stage_real_execute_finish (IdeBuildStage  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

const gchar *
ide_build_stage_get_name (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), NULL);

  return priv->name;
}

void
ide_build_stage_set_name (IdeBuildStage *self,
                          const gchar   *name)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  if (g_strcmp0 (name, priv->name) != 0)
    {
      g_free (priv->name);
      priv->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

static void
ide_build_stage_real_clean_async (IdeBuildStage       *self,
                                  IdeBuildPipeline    *pipeline,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_stage_real_clean_async);

  ide_build_stage_set_completed (self, FALSE);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_build_stage_real_clean_finish (IdeBuildStage  *self,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gboolean
ide_build_stage_real_chain (IdeBuildStage *self,
                            IdeBuildStage *next)
{
  return FALSE;
}

static void
ide_build_stage_finalize (GObject *object)
{
  IdeBuildStage *self = (IdeBuildStage *)object;
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  ide_build_stage_clear_observer (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->stdout_path, g_free);
  g_clear_object (&priv->queued_execute);
  g_clear_object (&priv->stdout_stream);

  G_OBJECT_CLASS (ide_build_stage_parent_class)->finalize (object);
}

static void
ide_build_stage_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeBuildStage *self = IDE_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, ide_build_stage_get_active (self));
      break;

    case PROP_CHECK_STDOUT:
      g_value_set_boolean (value, ide_build_stage_get_check_stdout (self));
      break;

    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_build_stage_get_completed (self));
      break;

    case PROP_DISABLED:
      g_value_set_boolean (value, ide_build_stage_get_disabled (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_build_stage_get_name (self));
      break;

    case PROP_STDOUT_PATH:
      g_value_set_string (value, ide_build_stage_get_stdout_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeBuildStage *self = IDE_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      ide_build_stage_set_active (self, g_value_get_boolean (value));
      break;

    case PROP_CHECK_STDOUT:
      ide_build_stage_set_check_stdout (self, g_value_get_boolean (value));
      break;

    case PROP_COMPLETED:
      ide_build_stage_set_completed (self, g_value_get_boolean (value));
      break;

    case PROP_DISABLED:
      ide_build_stage_set_disabled (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      ide_build_stage_set_name (self, g_value_get_string (value));
      break;

    case PROP_STDOUT_PATH:
      ide_build_stage_set_stdout_path (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_class_init (IdeBuildStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_stage_finalize;
  object_class->get_property = ide_build_stage_get_property;
  object_class->set_property = ide_build_stage_set_property;

  klass->execute = ide_build_stage_real_execute;
  klass->execute_async = ide_build_stage_real_execute_async;
  klass->execute_finish = ide_build_stage_real_execute_finish;
  klass->clean_async = ide_build_stage_real_clean_async;
  klass->clean_finish = ide_build_stage_real_clean_finish;
  klass->chain = ide_build_stage_real_chain;

  /**
   * IdeBuildStage:active:
   *
   * This property is set to %TRUE when the build stage is actively
   * running or cleaning.
   *
   * Since: 3.28
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the stage is actively running",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * IdeBuildStage:check-stdout:
   *
   * Most build systems will preserve stderr for the processes they call, such
   * as gcc, clang, and others. However, if your build system redirects all
   * output to stdout, you may need to set this property to %TRUE to ensure
   * that Builder will extract errors from stdout.
   *
   * One such example is Ninja.
   */
  properties [PROP_CHECK_STDOUT] =
    g_param_spec_boolean ("check-stdout",
                         "Check STDOUT",
                         "If STDOUT should be checked for errors using error regexes",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:completed:
   *
   * The "completed" property is set to %TRUE after the pipeline has
   * completed processing the stage. When the pipeline invalidates
   * phases, completed may be reset to %FALSE.
   */
  properties [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          "Completed",
                          "If the stage has been completed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:disabled:
   *
   * If the build stage is disabled. This allows you to have a stage that is
   * attached but will not be activated during execution.
   *
   * You may enable it later and then re-execute the pipeline.
   *
   * If the stage is both transient and disabled, it will not be removed during
   * the transient cleanup phase.
   */
  properties [PROP_DISABLED] =
    g_param_spec_boolean ("disabled",
                          "Disabled",
                          "If the stage has been disabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:name:
   *
   * The name of the build stage. This is only used by UI to view
   * the build pipeline.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The user visible name of the stage",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:stdout-path:
   *
   * The "stdout-path" property allows a build stage to redirect its log
   * messages to a stdout file. Instead of passing stdout along to the
   * build pipeline, they will be redirected to this file.
   *
   * For safety reasons, the contents are first redirected to a temporary
   * file and will be redirected to the stdout-path location after the
   * build stage has completed executing.
   */
  properties [PROP_STDOUT_PATH] =
    g_param_spec_string ("stdout-path",
                         "Stdout Path",
                         "Redirect standard output to this path",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:transient:
   *
   * If the build stage is transient.
   *
   * A transient build stage is removed after the completion of
   * ide_build_pipeline_execute_async(). This can be a convenient
   * way to add a temporary item to a build pipeline that should
   * be immediately discarded.
   */
  properties [PROP_TRANSIENT] =
    g_param_spec_boolean ("transient",
                          "Transient",
                          "If the stage should be removed after execution",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHAIN] =
    g_signal_new ("chain",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBuildStageClass, chain),
                  g_signal_accumulator_true_handled,
                  NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 1, IDE_TYPE_BUILD_STAGE);

  signals [QUERY] =
    g_signal_new ("query",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBuildStageClass, query),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_BUILD_PIPELINE,
                  G_TYPE_CANCELLABLE);

  signals [REAP] =
    g_signal_new ("reap",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBuildStageClass, reap),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, DZL_TYPE_DIRECTORY_REAPER);
}

static void
ide_build_stage_init (IdeBuildStage *self)
{
}

void
ide_build_stage_execute_async (IdeBuildStage       *self,
                               IdeBuildPipeline    *pipeline,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if G_UNLIKELY (priv->stdout_path != NULL)
    {
      g_autoptr(GFileOutputStream) stream = NULL;
      g_autoptr(GFile) file = NULL;
      g_autoptr(GError) error = NULL;

      file = g_file_new_for_path (priv->stdout_path);
      stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, &error);

      if (stream == NULL)
        {
          g_task_report_error (self, callback, user_data,
                               ide_build_stage_execute_async,
                               g_steal_pointer (&error));
          return;
        }

      g_clear_object (&priv->stdout_stream);

      priv->stdout_stream = G_OUTPUT_STREAM (g_steal_pointer (&stream));
    }

  IDE_BUILD_STAGE_GET_CLASS (self)->execute_async (self, pipeline, cancellable, callback, user_data);
}

gboolean
ide_build_stage_execute_finish (IdeBuildStage  *self,
                                GAsyncResult   *result,
                                GError        **error)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  /*
   * If for some reason execute_finish() is not called (likely due to use of
   * the build stage without a pipeline, so sort of a programming error) then
   * we won't clean up the stdout stream. But it gets cleaned up in finalize
   * anyway, so its safe (if only delayed rename()).
   *
   * We can just unref the stream, and the close will happen silently. We need
   * to do this as some async reads to be proxied to the stream may occur after
   * the execute_finish() completes.
   *
   * The Tail structure has it's own reference to stdout_stream.
   */
  g_clear_object (&priv->stdout_stream);

  return IDE_BUILD_STAGE_GET_CLASS (self)->execute_finish (self, result, error);
}

/**
 * ide_build_stage_set_log_observer:
 * @self: An #IdeBuildStage
 * @observer: (scope async): The observer for the log entries
 * @observer_data: data for @observer
 * @observer_data_destroy: destroy callback for @observer_data
 *
 * Sets the log observer to handle calls to the various stage logging
 * functions. This will be set by the pipeline to mux logs from all
 * stages into a unified build log.
 *
 * Plugins that need to handle logging from a build stage should set
 * an observer on the pipeline so that log distribution may be fanned
 * out to all observers.
 */
void
ide_build_stage_set_log_observer (IdeBuildStage       *self,
                                  IdeBuildLogObserver  observer,
                                  gpointer             observer_data,
                                  GDestroyNotify       observer_data_destroy)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  ide_build_stage_clear_observer (self);

  priv->observer = observer;
  priv->observer_data = observer_data;
  priv->observer_data_destroy = observer_data_destroy;
}

static void
ide_build_stage_log_internal (IdeBuildStage     *self,
                              IdeBuildLogStream  stream_type,
                              GOutputStream     *stream,
                              const gchar       *message,
                              gssize             message_len)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  /*
   * If we are logging to a file instead of the build pipeline, handle that
   * specially now and then exit without calling the observer.
   */
  if (stream != NULL)
    {
      gsize count;

      if G_UNLIKELY (message_len < 0)
        message_len = strlen (message);

      g_output_stream_write_all (stream, message, message_len, &count, NULL, NULL);
      g_output_stream_write_all (stream, "\n", 1, &count, NULL, NULL);

      return;
    }

  if G_LIKELY (priv->observer != NULL)
    priv->observer (stream_type, message, message_len, priv->observer_data);
}

void
ide_build_stage_log (IdeBuildStage     *self,
                     IdeBuildLogStream  stream_type,
                     const gchar       *message,
                     gssize             message_len)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  if (stream_type == IDE_BUILD_LOG_STDOUT)
    ide_build_stage_log_internal (self, stream_type, priv->stdout_stream, message, message_len);
  else
    ide_build_stage_log_internal (self, stream_type, NULL, message, message_len);
}

gboolean
ide_build_stage_get_completed (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->completed;
}

void
ide_build_stage_set_completed (IdeBuildStage *self,
                               gboolean       completed)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  completed = !!completed;

  if (completed != priv->completed)
    {
      priv->completed = completed;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETED]);
    }
}

void
ide_build_stage_set_transient (IdeBuildStage *self,
                               gboolean       transient)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  transient = !!transient;

  if (priv->transient != transient)
    {
      priv->transient = transient;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSIENT]);
    }
}

gboolean
ide_build_stage_get_transient (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->transient;
}

static void
ide_build_stage_observe_stream_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GDataInputStream *stream = (GDataInputStream *)object;
  g_autofree gchar *line = NULL;
  g_autoptr(GError) error = NULL;
  Tail *tail = user_data;
  gsize n_read = 0;

  g_assert (G_IS_DATA_INPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (tail != NULL);

  line = g_data_input_stream_read_line_finish_utf8 (stream, result, &n_read, &error);

  if (error == NULL)
    {
      if (line == NULL)
        goto cleanup;

      ide_build_stage_log_internal (tail->self, tail->stream_type, tail->stream, line, (gssize)n_read);

      if G_UNLIKELY (g_input_stream_is_closed (G_INPUT_STREAM (stream)))
        goto cleanup;

      g_data_input_stream_read_line_async (stream,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           ide_build_stage_observe_stream_cb,
                                           tail);

      return;
    }

  g_debug ("%s", error->message);

cleanup:
  tail_free (tail);
}


static void
ide_build_stage_observe_stream (IdeBuildStage     *self,
                                IdeBuildLogStream  stream_type,
                                GInputStream      *stream)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);
  g_autoptr(GDataInputStream) data_stream = NULL;
  Tail *tail;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (stream_type == IDE_BUILD_LOG_STDOUT || stream_type == IDE_BUILD_LOG_STDERR);
  g_assert (G_IS_INPUT_STREAM (stream));

  if (G_IS_DATA_INPUT_STREAM (stream))
    data_stream = g_object_ref (G_DATA_INPUT_STREAM (stream));
  else
    data_stream = g_data_input_stream_new (stream);

  IDE_TRACE_MSG ("Logging subprocess stream of type %s as %s",
                 G_OBJECT_TYPE_NAME (data_stream),
                 stream_type == IDE_BUILD_LOG_STDOUT ? "stdout" : "stderr");

  if (stream_type == IDE_BUILD_LOG_STDOUT)
    tail = tail_new (self, priv->stdout_stream, stream_type);
  else
    tail = tail_new (self, NULL, stream_type);

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_build_stage_observe_stream_cb,
                                       tail);
}

/**
 * ide_build_stage_log_subprocess:
 * @self: An #IdeBuildStage
 * @subprocess: An #IdeSubprocess
 *
 * This function will begin logging @subprocess by reading from the
 * stdout and stderr streams of the subprocess. You must have created
 * the subprocess with %G_SUBPROCESS_FLAGS_STDERR_PIPE and
 * %G_SUBPROCESS_FLAGS_STDOUT_PIPE so that the streams may be read.
 */
void
ide_build_stage_log_subprocess (IdeBuildStage *self,
                                IdeSubprocess *subprocess)
{
  GInputStream *stdout_stream;
  GInputStream *stderr_stream;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  stderr_stream = ide_subprocess_get_stderr_pipe (subprocess);
  stdout_stream = ide_subprocess_get_stdout_pipe (subprocess);

  if (stderr_stream != NULL)
    ide_build_stage_observe_stream (self, IDE_BUILD_LOG_STDERR, stderr_stream);

  if (stdout_stream != NULL)
    ide_build_stage_observe_stream (self, IDE_BUILD_LOG_STDOUT, stdout_stream);

  IDE_EXIT;
}

void
ide_build_stage_pause (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  g_atomic_int_inc (&priv->n_pause);
}

static void
ide_build_stage_unpause_execute_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBuildStage *self = (IdeBuildStage *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_stage_execute_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

void
ide_build_stage_unpause (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (priv->n_pause > 0);

  if (g_atomic_int_dec_and_test (&priv->n_pause) && priv->queued_execute != NULL)
    {
      g_autoptr(IdeTask) task = g_steal_pointer (&priv->queued_execute);
      GCancellable *cancellable = ide_task_get_cancellable (task);
      IdeBuildPipeline *pipeline = ide_task_get_task_data (task);

      g_assert (IDE_IS_TASK (task));
      g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
      g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

      if (priv->completed)
        {
          ide_task_return_boolean (task, TRUE);
          return;
        }

      ide_build_stage_execute_async (self,
                                     pipeline,
                                     cancellable,
                                     ide_build_stage_unpause_execute_cb,
                                     g_steal_pointer (&task));
    }
}

/**
 * _ide_build_stage_execute_with_query_async: (skip)
 *
 * This function is used to execute the build stage after emitting the
 * query signal. If the stage is paused after the query, execute will
 * be delayed until the correct number of ide_build_stage_unpause() calls
 * have occurred.
 */
void
_ide_build_stage_execute_with_query_async (IdeBuildStage       *self,
                                           IdeBuildPipeline    *pipeline,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_build_stage_execute_with_query_async);
  ide_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);

  if (priv->queued_execute != NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "A build is already in progress");
      return;
    }

  priv->queued_execute = g_steal_pointer (&task);

  /*
   * Pause the pipeline around our query call so that any call to
   * pause/unpause does not cause the stage to make progress. This allows
   * us to share the code-path to make progress on the build stage.
   */
  ide_build_stage_pause (self);
  g_signal_emit (self, signals [QUERY], 0, pipeline, cancellable);
  ide_build_stage_unpause (self);
}

gboolean
_ide_build_stage_execute_with_query_finish (IdeBuildStage  *self,
                                            GAsyncResult   *result,
                                            GError        **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

void
ide_build_stage_set_stdout_path (IdeBuildStage *self,
                                 const gchar   *stdout_path)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  if (g_strcmp0 (stdout_path, priv->stdout_path) != 0)
    {
      g_free (priv->stdout_path);
      priv->stdout_path = g_strdup (stdout_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STDOUT_PATH]);
    }
}

const gchar *
ide_build_stage_get_stdout_path (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), NULL);

  return priv->stdout_path;
}

gboolean
_ide_build_stage_has_query (IdeBuildStage *self)
{
  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  if (g_signal_has_handler_pending (self, signals [QUERY], 0, FALSE))
    IDE_RETURN (TRUE);

  if (IDE_BUILD_STAGE_GET_CLASS (self)->query)
    IDE_RETURN (TRUE);

  IDE_RETURN (FALSE);
}

void
ide_build_stage_clean_async (IdeBuildStage       *self,
                             IdeBuildPipeline    *pipeline,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_STAGE_GET_CLASS (self)->clean_async (self, pipeline, cancellable, callback, user_data);
}

gboolean
ide_build_stage_clean_finish (IdeBuildStage  *self,
                              GAsyncResult   *result,
                              GError        **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return IDE_BUILD_STAGE_GET_CLASS (self)->clean_finish (self, result, error);
}

void
ide_build_stage_emit_reap (IdeBuildStage      *self,
                           DzlDirectoryReaper *reaper)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (DZL_IS_DIRECTORY_REAPER (reaper));

  g_signal_emit (self, signals [REAP], 0, reaper);

  IDE_EXIT;
}

gboolean
ide_build_stage_chain (IdeBuildStage *self,
                       IdeBuildStage *next)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (next), FALSE);

  if (ide_build_stage_get_disabled (next))
    return FALSE;

  g_signal_emit (self, signals[CHAIN], 0, next, &ret);

  return ret;
}

gboolean
ide_build_stage_get_disabled (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->disabled;
}

void
ide_build_stage_set_disabled (IdeBuildStage *self,
                              gboolean       disabled)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  disabled = !!disabled;

  if (priv->disabled != disabled)
    {
      priv->disabled = disabled;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISABLED]);
    }
}

gboolean
ide_build_stage_get_check_stdout (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->check_stdout;
}

void
ide_build_stage_set_check_stdout (IdeBuildStage *self,
                                  gboolean       check_stdout)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  check_stdout = !!check_stdout;

  if (check_stdout != priv->check_stdout)
    {
      priv->check_stdout = check_stdout;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECK_STDOUT]);
    }
}

/**
 * ide_build_stage_get_active:
 * @self: a #IdeBuildStage
 *
 * Gets the "active" property, which is set to %TRUE when the
 * build stage is actively executing or cleaning.
 *
 * Returns: %TRUE if the stage is actively executing or cleaning.
 *
 * Since: 3.28
 */
gboolean
ide_build_stage_get_active (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->active;
}

void
ide_build_stage_set_active (IdeBuildStage *self,
                            gboolean       active)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  active = !!active;

  if (priv->active != active)
    {
      priv->active = active;
      ide_object_notify_in_main (IDE_OBJECT (self), properties [PROP_ACTIVE]);
    }
}

IdeBuildPhase
_ide_build_stage_get_phase (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), 0);

  return priv->phase;
}

void
_ide_build_stage_set_phase (IdeBuildStage *self,
                            IdeBuildPhase  phase)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  priv->phase = phase;
}
