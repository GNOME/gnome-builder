/* ide-pipeline-stage.c
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

#define G_LOG_DOMAIN "ide-pipeline-stage"

#include "config.h"

#include <string.h>

#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-pipeline.h"
#include "ide-pipeline-stage.h"
#include "ide-pipeline-stage-private.h"

typedef struct
{
  gchar               *name;
  IdeBuildLogObserver  observer;
  gpointer             observer_data;
  GDestroyNotify       observer_data_destroy;
  IdeTask             *queued_build;
  gchar               *stdout_path;
  GOutputStream       *stdout_stream;
  gint                 n_pause;
  IdePipelinePhase     phase;
  guint                completed : 1;
  guint                disabled : 1;
  guint                transient : 1;
  guint                check_stdout : 1;
  guint                active : 1;
} IdePipelineStagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdePipelineStage, ide_pipeline_stage, IDE_TYPE_OBJECT)

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
  IdePipelineStage     *self;
  GOutputStream     *stream;
  IdeBuildLogStream  stream_type;
} Tail;

static Tail *
tail_new (IdePipelineStage     *self,
          GOutputStream     *stream,
          IdeBuildLogStream  stream_type)
{
  Tail *tail;

  g_assert (IDE_IS_PIPELINE_STAGE (self));
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
ide_pipeline_stage_clear_observer (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);
  GDestroyNotify notify = priv->observer_data_destroy;
  gpointer data = priv->observer_data;

  priv->observer_data_destroy = NULL;
  priv->observer_data = NULL;
  priv->observer = NULL;

  if (notify != NULL)
    notify (data);
}

static gboolean
ide_pipeline_stage_real_build (IdePipelineStage  *self,
                               IdePipeline       *pipeline,
                               GCancellable      *cancellable,
                               GError           **error)
{
  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  return TRUE;
}

static void
ide_pipeline_stage_real_build_worker (IdeTask      *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  IdePipelineStage *self = source_object;
  IdePipeline *pipeline = task_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  if (IDE_PIPELINE_STAGE_GET_CLASS (self)->build (self, pipeline, cancellable, &error))
    ide_task_return_boolean (task, TRUE);
  else
    ide_task_return_error (task, g_steal_pointer (&error));
}

static void
ide_pipeline_stage_real_build_async (IdePipelineStage    *self,
                                     IdePipeline         *pipeline,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_PIPELINE (pipeline));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_real_build_async);
  ide_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);
  ide_task_run_in_thread (task, ide_pipeline_stage_real_build_worker);
}

static gboolean
ide_pipeline_stage_real_build_finish (IdePipelineStage  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

const gchar *
ide_pipeline_stage_get_name (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), NULL);

  return priv->name;
}

void
ide_pipeline_stage_set_name (IdePipelineStage *self,
                             const gchar      *name)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  if (g_set_str (&priv->name, name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

static void
ide_pipeline_stage_real_clean_async (IdePipelineStage    *self,
                                     IdePipeline         *pipeline,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_pipeline_stage_real_clean_async);

  ide_pipeline_stage_set_completed (self, FALSE);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_pipeline_stage_real_clean_finish (IdePipelineStage  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gboolean
ide_pipeline_stage_real_chain (IdePipelineStage *self,
                               IdePipelineStage *next)
{
  return FALSE;
}

static void
ide_pipeline_stage_finalize (GObject *object)
{
  IdePipelineStage *self = (IdePipelineStage *)object;
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  ide_pipeline_stage_clear_observer (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->stdout_path, g_free);
  g_clear_object (&priv->queued_build);
  g_clear_object (&priv->stdout_stream);

  G_OBJECT_CLASS (ide_pipeline_stage_parent_class)->finalize (object);
}

static void
ide_pipeline_stage_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdePipelineStage *self = IDE_PIPELINE_STAGE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, ide_pipeline_stage_get_active (self));
      break;

    case PROP_CHECK_STDOUT:
      g_value_set_boolean (value, ide_pipeline_stage_get_check_stdout (self));
      break;

    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_pipeline_stage_get_completed (self));
      break;

    case PROP_DISABLED:
      g_value_set_boolean (value, ide_pipeline_stage_get_disabled (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_pipeline_stage_get_name (self));
      break;

    case PROP_STDOUT_PATH:
      g_value_set_string (value, ide_pipeline_stage_get_stdout_path (self));
      break;

    case PROP_TRANSIENT:
      g_value_set_boolean (value, ide_pipeline_stage_get_transient (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdePipelineStage *self = IDE_PIPELINE_STAGE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      ide_pipeline_stage_set_active (self, g_value_get_boolean (value));
      break;

    case PROP_CHECK_STDOUT:
      ide_pipeline_stage_set_check_stdout (self, g_value_get_boolean (value));
      break;

    case PROP_COMPLETED:
      ide_pipeline_stage_set_completed (self, g_value_get_boolean (value));
      break;

    case PROP_DISABLED:
      ide_pipeline_stage_set_disabled (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      ide_pipeline_stage_set_name (self, g_value_get_string (value));
      break;

    case PROP_STDOUT_PATH:
      ide_pipeline_stage_set_stdout_path (self, g_value_get_string (value));
      break;

    case PROP_TRANSIENT:
      ide_pipeline_stage_set_transient (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_pipeline_stage_class_init (IdePipelineStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_pipeline_stage_finalize;
  object_class->get_property = ide_pipeline_stage_get_property;
  object_class->set_property = ide_pipeline_stage_set_property;

  klass->build = ide_pipeline_stage_real_build;
  klass->build_async = ide_pipeline_stage_real_build_async;
  klass->build_finish = ide_pipeline_stage_real_build_finish;
  klass->clean_async = ide_pipeline_stage_real_clean_async;
  klass->clean_finish = ide_pipeline_stage_real_clean_finish;
  klass->chain = ide_pipeline_stage_real_chain;

  /**
   * IdePipelineStage:active:
   *
   * This property is set to %TRUE when the build stage is actively
   * running or cleaning.
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the stage is actively running",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * IdePipelineStage:check-stdout:
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
   * IdePipelineStage:completed:
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
   * IdePipelineStage:disabled:
   *
   * If the build stage is disabled. This allows you to have a stage that is
   * attached but will not be activated during execution.
   *
   * You may enable it later and then re-build the pipeline.
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
   * IdePipelineStage:name:
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
   * IdePipelineStage:stdout-path:
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
   * IdePipelineStage:transient:
   *
   * If the build stage is transient.
   *
   * A transient build stage is removed after the completion of
   * ide_pipeline_build_async(). This can be a convenient
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

  /**
   * IdePipelineStage:chain:
   *
   * We might want to be able to "chain" multiple stages into a single stage
   * so that we can avoid duplicate work. For example, if we have a "make"
   * stage immediately follwed by a "make install" stage, it does not make
   * sense to perform them both individually.
   *
   * Returns: %TRUE if @next's work was chained into @self for the next
   *    execution of the pipeline.
   */
  signals [CHAIN] =
    g_signal_new ("chain",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdePipelineStageClass, chain),
                  g_signal_accumulator_true_handled,
                  NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_BOOLEAN, 1, IDE_TYPE_PIPELINE_STAGE);
  g_signal_set_va_marshaller (signals [CHAIN],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdePipelineStage::query:
   * @self: An #IdePipelineStage
   * @pipeline: An #IdePipeline
   * @targets: (element-type IdeBuildTarget) (nullable): an array
   *   of #IdeBuildTarget or %NULL
   * @cancellable: (nullable): a #GCancellable or %NULL
   *
   * The #IdePipelineStage::query signal is emitted to request that the
   * build stage update its completed stage from any external resources.
   *
   * This can be useful if you want to use an existing build stage instances
   * and use a signal to pause forward progress until an external system
   * has been checked.
   *
   * The targets that the user would like to ensure are built are provided
   * as @targets. Some #IdePipelineStage may use this to reduce the amount
   * of work they perform
   *
   * For example, in a signal handler, you may call ide_pipeline_stage_pause()
   * and perform an external operation. Forward progress of the stage will
   * be paused until a matching number of ide_pipeline_stage_unpause() calls
   * have been made.
   */
  signals [QUERY] =
    g_signal_new ("query",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdePipelineStageClass, query),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT_BOXED_OBJECT,
                  G_TYPE_NONE,
                  3,
                  IDE_TYPE_PIPELINE,
                  G_TYPE_PTR_ARRAY,
                  G_TYPE_CANCELLABLE);
  g_signal_set_va_marshaller (signals [QUERY],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECT_BOXED_OBJECTv);

  /**
   * IdePipelineStage::reap:
   * @self: An #IdePipelineStage
   * @reaper: An #IdeDirectoryReaper
   *
   * This signal is emitted when a request to rebuild the project has
   * occurred. This allows build stages to ensure that certain files are
   * removed from the system. For example, an autotools build stage might
   * request that "configure" is removed so that autogen.sh will be Executed
   * as part of the next build.
   */
  signals [REAP] =
    g_signal_new ("reap",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdePipelineStageClass, reap),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_DIRECTORY_REAPER);
  g_signal_set_va_marshaller (signals [REAP],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_pipeline_stage_init (IdePipelineStage *self)
{
}

void
ide_pipeline_stage_build_async (IdePipelineStage    *self,
                                  IdePipeline         *pipeline,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
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
                               ide_pipeline_stage_build_async,
                               g_steal_pointer (&error));
          return;
        }

      g_clear_object (&priv->stdout_stream);

      priv->stdout_stream = G_OUTPUT_STREAM (g_steal_pointer (&stream));
    }

  IDE_PIPELINE_STAGE_GET_CLASS (self)->build_async (self, pipeline, cancellable, callback, user_data);
}

gboolean
ide_pipeline_stage_build_finish (IdePipelineStage  *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  /*
   * If for some reason build_finish() is not called (likely due to use of
   * the build stage without a pipeline, so sort of a programming error) then
   * we won't clean up the stdout stream. But it gets cleaned up in finalize
   * anyway, so its safe (if only delayed rename()).
   *
   * We can just unref the stream, and the close will happen silently. We need
   * to do this as some async reads to be proxied to the stream may occur after
   * the build_finish() completes.
   *
   * The Tail structure has it's own reference to stdout_stream.
   */
  g_clear_object (&priv->stdout_stream);

  return IDE_PIPELINE_STAGE_GET_CLASS (self)->build_finish (self, result, error);
}

/**
 * ide_pipeline_stage_set_log_observer:
 * @self: An #IdePipelineStage
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
ide_pipeline_stage_set_log_observer (IdePipelineStage    *self,
                                     IdeBuildLogObserver  observer,
                                     gpointer             observer_data,
                                     GDestroyNotify       observer_data_destroy)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  ide_pipeline_stage_clear_observer (self);

  priv->observer = observer;
  priv->observer_data = observer_data;
  priv->observer_data_destroy = observer_data_destroy;
}

static void
ide_pipeline_stage_log_internal (IdePipelineStage  *self,
                                 IdeBuildLogStream  stream_type,
                                 GOutputStream     *stream,
                                 const gchar       *message,
                                 gssize             message_len)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

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
ide_pipeline_stage_log (IdePipelineStage  *self,
                        IdeBuildLogStream  stream_type,
                        const gchar       *message,
                        gssize             message_len)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  if (stream_type == IDE_BUILD_LOG_STDOUT)
    ide_pipeline_stage_log_internal (self, stream_type, priv->stdout_stream, message, message_len);
  else
    ide_pipeline_stage_log_internal (self, stream_type, NULL, message, message_len);
}

gboolean
ide_pipeline_stage_get_completed (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  return priv->completed;
}

void
ide_pipeline_stage_set_completed (IdePipelineStage *self,
                                  gboolean          completed)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  completed = !!completed;

  if (completed != priv->completed)
    {
      priv->completed = completed;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETED]);
    }
}

void
ide_pipeline_stage_set_transient (IdePipelineStage *self,
                                  gboolean          transient)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  transient = !!transient;

  if (priv->transient != transient)
    {
      priv->transient = transient;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSIENT]);
    }
}

gboolean
ide_pipeline_stage_get_transient (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  return priv->transient;
}

static void
ide_pipeline_stage_observe_stream_cb (GObject      *object,
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

      ide_pipeline_stage_log_internal (tail->self, tail->stream_type, tail->stream, line, (gssize)n_read);

      if G_UNLIKELY (g_input_stream_is_closed (G_INPUT_STREAM (stream)))
        goto cleanup;

      g_data_input_stream_read_line_async (stream,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           ide_pipeline_stage_observe_stream_cb,
                                           tail);

      return;
    }

  g_debug ("%s", error->message);

cleanup:
  tail_free (tail);
}


static void
ide_pipeline_stage_observe_stream (IdePipelineStage  *self,
                                   IdeBuildLogStream  stream_type,
                                   GInputStream      *stream)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);
  g_autoptr(GDataInputStream) data_stream = NULL;
  Tail *tail;

  g_assert (IDE_IS_PIPELINE_STAGE (self));
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
                                       ide_pipeline_stage_observe_stream_cb,
                                       tail);
}

/**
 * ide_pipeline_stage_log_subprocess:
 * @self: An #IdePipelineStage
 * @subprocess: An #IdeSubprocess
 *
 * This function will begin logging @subprocess by reading from the
 * stdout and stderr streams of the subprocess. You must have created
 * the subprocess with %G_SUBPROCESS_FLAGS_STDERR_PIPE and
 * %G_SUBPROCESS_FLAGS_STDOUT_PIPE so that the streams may be read.
 */
void
ide_pipeline_stage_log_subprocess (IdePipelineStage *self,
                                   IdeSubprocess    *subprocess)
{
  GInputStream *stdout_stream;
  GInputStream *stderr_stream;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  stderr_stream = ide_subprocess_get_stderr_pipe (subprocess);
  stdout_stream = ide_subprocess_get_stdout_pipe (subprocess);

  if (stderr_stream != NULL)
    ide_pipeline_stage_observe_stream (self, IDE_BUILD_LOG_STDERR, stderr_stream);

  if (stdout_stream != NULL)
    ide_pipeline_stage_observe_stream (self, IDE_BUILD_LOG_STDOUT, stdout_stream);

  IDE_EXIT;
}

void
ide_pipeline_stage_pause (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  g_atomic_int_inc (&priv->n_pause);
}

static void
ide_pipeline_stage_unpause_build_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdePipelineStage *self = (IdePipelineStage *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_PIPELINE_STAGE (self));
  g_assert (IDE_IS_TASK (task));

  if (!ide_pipeline_stage_build_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

void
ide_pipeline_stage_unpause (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (priv->n_pause > 0);

  if (g_atomic_int_dec_and_test (&priv->n_pause) && priv->queued_build != NULL)
    {
      g_autoptr(IdeTask) task = g_steal_pointer (&priv->queued_build);
      GCancellable *cancellable = ide_task_get_cancellable (task);
      IdePipeline *pipeline = ide_task_get_task_data (task);

      g_assert (IDE_IS_TASK (task));
      g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
      g_assert (IDE_IS_PIPELINE (pipeline));

      if (priv->completed)
        {
          ide_task_return_boolean (task, TRUE);
          return;
        }

      ide_pipeline_stage_build_async (self,
                                      pipeline,
                                      cancellable,
                                      ide_pipeline_stage_unpause_build_cb,
                                      g_steal_pointer (&task));
    }
}

/**
 * _ide_pipeline_stage_build_with_query_async: (skip)
 *
 * This function is used to build the build stage after emitting the
 * query signal. If the stage is paused after the query, build will
 * be delayed until the correct number of ide_pipeline_stage_unpause() calls
 * have occurred.
 */
void
_ide_pipeline_stage_build_with_query_async (IdePipelineStage    *self,
                                            IdePipeline         *pipeline,
                                            GPtrArray           *targets,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);
  g_autoptr(GPtrArray) local_targets = NULL;
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_pipeline_stage_build_with_query_async);
  ide_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);

  if (targets == NULL)
    targets = local_targets = g_ptr_array_new_with_free_func (g_object_unref);

  if (priv->queued_build != NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "A build is already in progress");
      return;
    }

  priv->queued_build = g_steal_pointer (&task);

  /*
   * Pause the pipeline around our query call so that any call to
   * pause/unpause does not cause the stage to make progress. This allows
   * us to share the code-path to make progress on the build stage.
   */
  ide_pipeline_stage_pause (self);
  g_signal_emit (self, signals [QUERY], 0, pipeline, targets, cancellable);
  ide_pipeline_stage_unpause (self);
}

gboolean
_ide_pipeline_stage_build_with_query_finish (IdePipelineStage  *self,
                                             GAsyncResult      *result,
                                             GError           **error)
{
  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

void
ide_pipeline_stage_set_stdout_path (IdePipelineStage *self,
                                    const gchar      *stdout_path)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  if (g_set_str (&priv->stdout_path, stdout_path))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STDOUT_PATH]);
    }
}

const gchar *
ide_pipeline_stage_get_stdout_path (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), NULL);

  return priv->stdout_path;
}

gboolean
_ide_pipeline_stage_has_query (IdePipelineStage *self)
{
  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  if (g_signal_has_handler_pending (self, signals [QUERY], 0, FALSE))
    IDE_RETURN (TRUE);

  if (IDE_PIPELINE_STAGE_GET_CLASS (self)->query)
    IDE_RETURN (TRUE);

  IDE_RETURN (FALSE);
}

void
ide_pipeline_stage_clean_async (IdePipelineStage    *self,
                                IdePipeline         *pipeline,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PIPELINE_STAGE_GET_CLASS (self)->clean_async (self, pipeline, cancellable, callback, user_data);
}

gboolean
ide_pipeline_stage_clean_finish (IdePipelineStage  *self,
                                 GAsyncResult      *result,
                                 GError           **error)
{
  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return IDE_PIPELINE_STAGE_GET_CLASS (self)->clean_finish (self, result, error);
}

void
ide_pipeline_stage_emit_reap (IdePipelineStage   *self,
                              IdeDirectoryReaper *reaper)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));
  g_return_if_fail (IDE_IS_DIRECTORY_REAPER (reaper));

  g_signal_emit (self, signals [REAP], 0, reaper);

  IDE_EXIT;
}

gboolean
ide_pipeline_stage_chain (IdePipelineStage *self,
                          IdePipelineStage *next)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (next), FALSE);

  if (ide_pipeline_stage_get_disabled (next))
    return FALSE;

  g_signal_emit (self, signals[CHAIN], 0, next, &ret);

  return ret;
}

gboolean
ide_pipeline_stage_get_disabled (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  return priv->disabled;
}

void
ide_pipeline_stage_set_disabled (IdePipelineStage *self,
                                 gboolean          disabled)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  disabled = !!disabled;

  if (priv->disabled != disabled)
    {
      priv->disabled = disabled;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISABLED]);
    }
}

gboolean
ide_pipeline_stage_get_check_stdout (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  return priv->check_stdout;
}

void
ide_pipeline_stage_set_check_stdout (IdePipelineStage *self,
                                     gboolean          check_stdout)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  check_stdout = !!check_stdout;

  if (check_stdout != priv->check_stdout)
    {
      priv->check_stdout = check_stdout;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECK_STDOUT]);
    }
}

/**
 * ide_pipeline_stage_get_active:
 * @self: a #IdePipelineStage
 *
 * Gets the "active" property, which is set to %TRUE when the
 * build stage is actively executing or cleaning.
 *
 * Returns: %TRUE if the stage is actively executing or cleaning.
 */
gboolean
ide_pipeline_stage_get_active (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), FALSE);

  return priv->active;
}

void
ide_pipeline_stage_set_active (IdePipelineStage *self,
                               gboolean          active)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  active = !!active;

  if (priv->active != active)
    {
      priv->active = active;
      ide_object_notify_in_main (IDE_OBJECT (self), properties [PROP_ACTIVE]);
    }
}

IdePipelinePhase
_ide_pipeline_stage_get_phase (IdePipelineStage *self)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PIPELINE_STAGE (self), 0);

  return priv->phase;
}

void
_ide_pipeline_stage_set_phase (IdePipelineStage *self,
                               IdePipelinePhase     phase)
{
  IdePipelineStagePrivate *priv = ide_pipeline_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_PIPELINE_STAGE (self));

  priv->phase = phase;
}
