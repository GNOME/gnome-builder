/* ide-build-result.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "ide-build-result.h"

typedef struct
{
  GMutex         mutex;

  GInputStream  *stdout_reader;
  GOutputStream *stdout_writer;

  GInputStream  *stderr_reader;
  GOutputStream *stderr_writer;
} IdeBuildResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildResult, ide_build_result, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

//static GParamSpec *gParamSpecs [LAST_PROP];

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

static void
_ide_build_result_log (GOutputStream  *stream,
                       const gchar    *message)
{
  g_autofree gchar *buffer = NULL;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (message);

  /*
   * TODO: Is there a better way we can do this to just add a newline
   *       at the end of stuff? Previously, I had a bunch of log stuff here
   *       like date/time, but I didn't think it was necessary in the long
   *       run. Would be nice to remove the printf as well. We need to do the
   *       write as a single item in case we have multiple threads appending.
   */

  buffer = g_strdup_printf ("%s\n", message);
  g_output_stream_write_all (stream, buffer, strlen (buffer),
                             NULL, NULL, NULL);
}

void
ide_build_result_log_stdout (IdeBuildResult *self,
                             const gchar    *format,
                             ...)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  g_autofree gchar *msg = NULL;
  va_list args;

  /* lazy create stream if necessary */
  (void)ide_build_result_get_stdout_stream (self);

  if (priv->stdout_writer)
    {
      va_start (args, format);
      msg = g_strdup_vprintf (format, args);
      va_end (args);

      _ide_build_result_log (priv->stdout_writer, msg);
    }
}

void
ide_build_result_log_stderr (IdeBuildResult *self,
                             const gchar    *format,
                             ...)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);
  g_autofree gchar *msg = NULL;
  va_list args;

  /* lazy create stream if necessary */
  (void)ide_build_result_get_stderr_stream (self);

  if (priv->stderr_writer)
    {
      va_start (args, format);
      msg = g_strdup_vprintf (format, args);
      va_end (args);

      _ide_build_result_log (priv->stderr_writer, msg);
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
  g_autoptr(GOutputStream) writer = user_data;
  g_autofree gchar *line = NULL;
  g_autoptr(GError) error = NULL;
  gsize n_read;

  g_return_if_fail (G_IS_INPUT_STREAM (reader));
  g_return_if_fail (G_IS_OUTPUT_STREAM (writer));

  line = g_data_input_stream_read_line_finish_utf8 (reader, result, &n_read,
                                                    &error);

  if (line)
    {
      _ide_build_result_log (writer, line);
      g_data_input_stream_read_line_async (reader,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           ide_build_result_tail_cb,
                                           g_object_ref (writer));
    }
}

static void
ide_build_result_tail_into (IdeBuildResult *self,
                            GInputStream   *reader,
                            GOutputStream  *writer)
{
  g_autoptr(GDataInputStream) data_reader = NULL;

  g_return_if_fail (IDE_IS_BUILD_RESULT (self));
  g_return_if_fail (G_IS_INPUT_STREAM (reader));
  g_return_if_fail (G_IS_OUTPUT_STREAM (writer));

  data_reader = g_data_input_stream_new (reader);

  g_data_input_stream_read_line_async (data_reader,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_build_result_tail_cb,
                                       g_object_ref (writer));
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
    ide_build_result_tail_into (self, stderr_stream, priv->stderr_writer);

  stdout_stream = g_subprocess_get_stdout_pipe (subprocess);
  if (stdout_stream)
    ide_build_result_tail_into (self, stdout_stream, priv->stdout_writer);
}

static void
ide_build_result_finalize (GObject *object)
{
  IdeBuildResult *self = (IdeBuildResult *)object;
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_mutex_clear (&priv->mutex);

  g_clear_object (&priv->stderr_reader);
  g_clear_object (&priv->stderr_writer);

  g_clear_object (&priv->stdout_reader);
  g_clear_object (&priv->stdout_writer);

  G_OBJECT_CLASS (ide_build_result_parent_class)->finalize (object);
}

static void
ide_build_result_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  //IdeBuildResult *self = IDE_BUILD_RESULT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_result_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  //IdeBuildResult *self = IDE_BUILD_RESULT (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_result_class_init (IdeBuildResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_result_finalize;
  object_class->get_property = ide_build_result_get_property;
  object_class->set_property = ide_build_result_set_property;
}

static void
ide_build_result_init (IdeBuildResult *self)
{
  IdeBuildResultPrivate *priv = ide_build_result_get_instance_private (self);

  g_mutex_init (&priv->mutex);
}
