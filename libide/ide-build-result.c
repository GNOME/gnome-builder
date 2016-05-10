/* ide-build-result.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libpeas/peas.h>

#include "ide-build-result.h"
#include "ide-build-result-addin.h"
#include "ide-enums.h"
#include "ide-file.h"
#include "ide-source-location.h"

#define POINTER_MARK(p)   GSIZE_TO_POINTER(GPOINTER_TO_SIZE(p)|1)
#define POINTER_UNMARK(p) GSIZE_TO_POINTER(GPOINTER_TO_SIZE(p)&~(gsize)1)
#define POINTER_MARKED(p) (GPOINTER_TO_SIZE(p)&1)

typedef struct
{
  GMutex            mutex;

  GInputStream     *stdout_reader;
  GOutputStream    *stdout_writer;

  GInputStream     *stderr_reader;
  GOutputStream    *stderr_writer;

  PeasExtensionSet *addins;

  GSource          *log_source;
  GAsyncQueue      *log_queue;

  GTimer           *timer;
  gchar            *mode;

  guint             running : 1;
} IdeBuildResultPrivate;

typedef struct
{
  IdeBuildResult    *self;
  GOutputStream     *writer;
  IdeBuildResultLog  log;
} Tail;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildResult, ide_build_result, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_MODE,
  PROP_RUNNING,
  LAST_PROP
};

enum {
  DIAGNOSTIC,
  LOG,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static gboolean
_ide_build_result_open_log (IdeBuildResult  *self,
                            GInputStream   **read_stream,
                            GOutputStream  **write_stream,
                            const gchar     *template)
{
  g_autofree gchar *name_used = NULL;
  gint fd;

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), FALSE);
  g_return_val_if_fail (read_stream, FALSE);
  g_return_val_if_fail (write_stream, FALSE);

  fd = g_file_open_tmp (template, &name_used, NULL);

  if (fd != -1)
    {
      g_autoptr(GFile) file = NULL;

      file = g_file_new_for_path (name_used);
      *read_stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
      *write_stream = g_unix_output_stream_new (fd, TRUE);
      g_unlink (name_used);

      return TRUE;
    }

  return FALSE;
}

void
_ide_build_result_log (IdeBuildResult    *self,
                       GSource           *source,
                       GAsyncQueue       *queue,
                       GOutputStream     *stream,
                       IdeBuildResultLog  log,
                       const gchar       *format,
                       va_list            args)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  g_autofree gchar *freeme = NULL;
  gchar data[256];
  gchar *message = data;
  va_list copy;
  gint len;

  g_assert (source != NULL);
  g_assert (queue != NULL);
  g_assert (G_IS_OUTPUT_STREAM (stream));
  g_assert (message != NULL);

  G_VA_COPY (copy, args);
  len = g_vsnprintf (data, sizeof data, format, copy);
  va_end (copy);

  if (len >= sizeof data - 1)
    {
      freeme = g_malloc (len + 2);
      g_vsnprintf (freeme, len + 2, format, args);
      message = freeme;
    }

  message [len++] = '\n';
  message [len] = '\0';

  g_output_stream_write_all (stream, message, len, NULL, NULL, NULL);

  if G_UNLIKELY (g_source_get_context (source) != g_main_context_get_thread_default ())
    {
      gchar *copied;

      if G_UNLIKELY (freeme != NULL)
        copied = g_steal_pointer (&freeme);
      else
        copied = g_strdup (message);

      if G_UNLIKELY (log == IDE_BUILD_RESULT_LOG_STDERR)
        copied = POINTER_MARK (copied);

      g_async_queue_push (priv->log_queue, copied);
      g_source_set_ready_time (source, 0);
    }
  else
    {
      g_signal_emit (self, signals [LOG], 0, log, message);
    }
}

void
ide_build_result_log_stdout (IdeBuildResult *self,
                             const gchar    *format,
                             ...)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  G_GNUC_UNUSED GInputStream *stream;
  va_list args;

  /* lazy create stream if necessary */
  stream = ide_build_result_get_stdout_stream (self);

  if (priv->stdout_writer)
    {
      va_start (args, format);
      _ide_build_result_log (self,
                             priv->log_source,
                             priv->log_queue,
                             priv->stdout_writer,
                             IDE_BUILD_RESULT_LOG_STDOUT,
                             format,
                             args);
      va_end (args);
    }
}

void
ide_build_result_log_stderr (IdeBuildResult *self,
                             const gchar    *format,
                             ...)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  G_GNUC_UNUSED GInputStream *stream;
  va_list args;

  /* lazy create stream if necessary */
  stream = ide_build_result_get_stderr_stream (self);

  if (priv->stderr_writer)
    {
      va_start (args, format);
      _ide_build_result_log (self,
                             priv->log_source,
                             priv->log_queue,
                             priv->stderr_writer,
                             IDE_BUILD_RESULT_LOG_STDERR,
                             format,
                             args);
      va_end (args);
    }
}

/**
 * ide_build_result_get_stderr_stream:
 *
 * Fetches a merged stdedrr stream for all child processes of this build result.
 *
 * Returns: (transfer none): A #GInputStream.
 */
GInputStream *
ide_build_result_get_stderr_stream (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), NULL);

  g_mutex_lock (&priv->mutex);

  if (!priv->stderr_reader)
    {
      if (!_ide_build_result_open_log (self,
                                       &priv->stderr_reader,
                                       &priv->stderr_writer,
                                       "libide-XXXXXX.stderr.log"))
        g_warning (_("Failed to open stderr stream."));
    }

  g_mutex_unlock (&priv->mutex);

  return priv->stderr_reader;
}

/**
 * ide_build_result_get_stdout_stream:
 *
 * Fetches a merged stdout stream for all child processes of this build result.
 *
 * Returns: (transfer none) (nullable): A #GInputStream or %NULL.
 */
GInputStream *
ide_build_result_get_stdout_stream (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), NULL);

  g_mutex_lock (&priv->mutex);

  if (!priv->stdout_reader)
    {
      if (!_ide_build_result_open_log (self,
                                       &priv->stdout_reader,
                                       &priv->stdout_writer,
                                       "libide-XXXXXX.stdout.log"))
        g_warning (_("Failed to open stdout stream."));
    }

  g_mutex_unlock (&priv->mutex);

  return priv->stdout_reader;
}

static void
ide_build_result_tail_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GDataInputStream *reader = (GDataInputStream *)object;
  g_autofree gchar *line = NULL;
  g_autoptr(GError) error = NULL;
  Tail *tail = user_data;
  gsize n_read;

  g_assert (G_IS_INPUT_STREAM (reader));
  g_assert (tail != NULL);
  g_assert (G_IS_OUTPUT_STREAM (tail->writer));

  line = g_data_input_stream_read_line_finish_utf8 (reader, result, &n_read, &error);

  if (line)
    {
      if (tail->log == IDE_BUILD_RESULT_LOG_STDOUT)
        ide_build_result_log_stdout (tail->self, "%s", line);
      else
        ide_build_result_log_stderr (tail->self, "%s", line);

      g_data_input_stream_read_line_async (reader,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           ide_build_result_tail_cb,
                                           tail);
    }
  else
    {
      g_object_unref (tail->self);
      g_object_unref (tail->writer);
      g_slice_free1 (sizeof *tail, tail);
    }
}

static void
ide_build_result_tail_into (IdeBuildResult    *self,
                            IdeBuildResultLog  log,
                            GInputStream      *reader,
                            GOutputStream     *writer)
{
  g_autoptr(GDataInputStream) data_reader = NULL;
  Tail *tail;

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));
  g_return_if_fail (G_IS_INPUT_STREAM (reader));
  g_return_if_fail (G_IS_OUTPUT_STREAM (writer));

  data_reader = g_data_input_stream_new (reader);

  tail = g_slice_alloc0 (sizeof *tail);
  tail->self = g_object_ref (self);
  tail->writer = g_object_ref (writer);
  tail->log = log;

  g_data_input_stream_read_line_async (data_reader,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_build_result_tail_cb,
                                       tail);
}

void
ide_build_result_log_subprocess (IdeBuildResult *self,
                                 GSubprocess    *subprocess)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  GInputStream *stdout_stream;
  GInputStream *stderr_stream;

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));
  g_return_if_fail (G_IS_SUBPROCESS (subprocess));

  /* ensure lazily created streams are available */
  (void)ide_build_result_get_stderr_stream (self);
  (void)ide_build_result_get_stdout_stream (self);

  stderr_stream = g_subprocess_get_stderr_pipe (subprocess);
  if (stderr_stream)
    ide_build_result_tail_into (self,
                                IDE_BUILD_RESULT_LOG_STDERR,
                                stderr_stream,
                                priv->stderr_writer);

  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);
  if (stdout_stream)
    ide_build_result_tail_into (self,
                                IDE_BUILD_RESULT_LOG_STDOUT,
                                stdout_stream,
                                priv->stdout_writer);
}

static void
ide_build_result_addin_added (PeasExtensionSet    *set,
                              PeasPluginInfo      *plugin_info,
                              IdeBuildResultAddin *addin,
                              IdeBuildResult      *self)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_RESULT_ADDIN (addin));
  g_assert (IDE_IS_BUILD_RESULT (self));

  if (IDE_IS_OBJECT (addin))
    ide_object_set_context (IDE_OBJECT (addin),
                            ide_object_get_context (IDE_OBJECT (self)));

  ide_build_result_addin_load (addin, self);
}

static void
ide_build_result_addin_removed (PeasExtensionSet    *set,
                                PeasPluginInfo      *plugin_info,
                                IdeBuildResultAddin *addin,
                                IdeBuildResult      *self)
{
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_BUILD_RESULT_ADDIN (addin));
  g_assert (IDE_IS_BUILD_RESULT (self));

  ide_build_result_addin_unload (addin, self);
}

static gboolean
emit_log_from_main (gpointer user_data)
{
  IdeBuildResult *self = user_data;
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  g_autoptr(GPtrArray) ar = g_ptr_array_new ();
  gpointer item;
  guint i;

  g_assert (IDE_IS_BUILD_RESULT (self));

  g_async_queue_lock (priv->log_queue);
  while (NULL != (item = g_async_queue_try_pop_unlocked (priv->log_queue)))
    g_ptr_array_add (ar, item);
  g_source_set_ready_time (priv->log_source, -1);
  g_async_queue_unlock (priv->log_queue);

  for (i = 0; i < ar->len; i++)
    {
      IdeBuildResultLog log = IDE_BUILD_RESULT_LOG_STDOUT;
      gchar *message;

      item = g_ptr_array_index (ar, i);
      message = POINTER_UNMARK (item);

      if (POINTER_MARKED (item))
        log = IDE_BUILD_RESULT_LOG_STDERR;

      g_signal_emit (self, signals[LOG], 0, log, message);

      g_free (message);
    }

  return G_SOURCE_CONTINUE;
}

static void
ide_build_result_constructed (GObject *object)
{
  IdeBuildResult *self = (IdeBuildResult *)object;
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  PeasEngine *engine = peas_engine_get_default ();

  G_OBJECT_CLASS (ide_build_result_parent_class)->constructed (object);

  priv->addins = peas_extension_set_new (engine,
                                         IDE_TYPE_BUILD_RESULT_ADDIN,
                                         NULL);
  peas_extension_set_foreach (priv->addins,
                              (PeasExtensionSetForeachFunc)ide_build_result_addin_added,
                              self);
  g_signal_connect_object (priv->addins,
                           "extension-added",
                           G_CALLBACK (ide_build_result_addin_added),
                           self,
                           0);
  g_signal_connect_object (priv->addins,
                           "extension-removed",
                           G_CALLBACK (ide_build_result_addin_removed),
                           self,
                           0);
}

static void
ide_build_result_finalize (GObject *object)
{
  IdeBuildResult *self = (IdeBuildResult *)object;
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_clear_object (&priv->addins);

  g_clear_object (&priv->stderr_reader);
  g_clear_object (&priv->stderr_writer);

  g_clear_object (&priv->stdout_reader);
  g_clear_object (&priv->stdout_writer);

  g_clear_pointer (&priv->mode, g_free);
  g_clear_pointer (&priv->timer, g_timer_destroy);

  g_clear_pointer (&priv->log_source, g_source_destroy);

  g_mutex_clear (&priv->mutex);

  G_OBJECT_CLASS (ide_build_result_parent_class)->finalize (object);
}

static void
ide_build_result_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeBuildResult *self = IDE_BUILD_RESULT(object);

  switch (prop_id)
    {
    case PROP_RUNNING:
      g_value_set_boolean (value, ide_build_result_get_running (self));
      break;

    case PROP_MODE:
      g_value_take_string (value, ide_build_result_get_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_build_result_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeBuildResult *self = IDE_BUILD_RESULT(object);

  switch (prop_id)
    {
    case PROP_RUNNING:
      ide_build_result_set_running (self, g_value_get_boolean (value));
      break;

    case PROP_MODE:
      ide_build_result_set_mode (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_build_result_class_init (IdeBuildResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_build_result_constructed;
  object_class->finalize = ide_build_result_finalize;
  object_class->get_property = ide_build_result_get_property;
  object_class->set_property = ide_build_result_set_property;

  properties [PROP_MODE] =
    g_param_spec_string ("mode",
                         "Mode",
                         "The name of the current build step",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  properties [PROP_RUNNING] =
    g_param_spec_boolean ("running",
                          "Running",
                          "If the build process is still running.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [DIAGNOSTIC] =
    g_signal_new ("diagnostic",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBuildResultClass, diagnostic),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DIAGNOSTIC);

  signals [LOG] =
    g_signal_new ("log",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeBuildResultClass, log),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, IDE_TYPE_BUILD_RESULT_LOG, G_TYPE_STRING);
}

static void
ide_build_result_init (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_mutex_init (&priv->mutex);

  priv->timer = g_timer_new ();

  priv->log_queue = g_async_queue_new ();

  priv->log_source = g_timeout_source_new (G_MAXINT);
  g_source_set_ready_time (priv->log_source, -1);
  g_source_set_name (priv->log_source, "[ide] build_logs");
  g_source_set_callback (priv->log_source, emit_log_from_main, self, NULL);
  g_source_attach (priv->log_source, g_main_context_default ());
}

GTimeSpan
ide_build_result_get_running_time (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), 0);

  return g_timer_elapsed (priv->timer, NULL) * G_USEC_PER_SEC;
}

gchar *
ide_build_result_get_mode (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  gchar *copy;

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), NULL);

  g_mutex_lock (&priv->mutex);
  copy = g_strdup (priv->mode);
  g_mutex_unlock (&priv->mutex);

  return copy;
}

void
ide_build_result_set_mode (IdeBuildResult *self,
                           const gchar    *mode)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));

  g_mutex_lock (&priv->mutex);
  if (!ide_str_equal0 (priv->mode, mode))
    {
      g_free (priv->mode);
      priv->mode = g_strdup (mode);
      ide_object_notify_in_main (self, properties [PROP_MODE]);
    }
  g_mutex_unlock (&priv->mutex);
}

gboolean
ide_build_result_get_running (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_RESULT (self), FALSE);

  return priv->running;
}

void
ide_build_result_set_running (IdeBuildResult *self,
                              gboolean        running)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));

  running = !!running;

  g_mutex_lock (&priv->mutex);
  if (priv->running != running)
    {
      priv->running = running;
      if (!running)
        g_timer_stop (priv->timer);
      ide_object_notify_in_main (self, properties [PROP_RUNNING]);
    }
  g_mutex_unlock (&priv->mutex);
}

static gboolean
ide_build_result_emit_diagnostic_cb (gpointer data)
{
  struct {
    IdeBuildResult *result;
    IdeDiagnostic  *diagnostic;
  } *pair = data;

  g_assert (pair != NULL);
  g_assert (IDE_IS_BUILD_RESULT (pair->result));
  g_assert (pair->diagnostic != NULL);

  g_signal_emit (pair->result, signals [DIAGNOSTIC], 0, pair->diagnostic);

  g_object_unref (pair->result);
  ide_diagnostic_unref (pair->diagnostic);
  g_slice_free1 (sizeof *pair, pair);

  return G_SOURCE_REMOVE;
}

void
ide_build_result_emit_diagnostic (IdeBuildResult *self,
                                  IdeDiagnostic  *diagnostic)
{
  struct {
    IdeBuildResult *result;
    IdeDiagnostic  *diagnostic;
  } *pair;

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));
  g_return_if_fail (diagnostic != NULL);

  pair = g_slice_alloc0 (sizeof *pair);
  pair->result = g_object_ref (self);
  pair->diagnostic = ide_diagnostic_ref (diagnostic);

  g_timeout_add (0, ide_build_result_emit_diagnostic_cb, pair);
}
