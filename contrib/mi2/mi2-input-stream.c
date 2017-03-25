/* mi2-input-stream.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "mi2-input-stream"

#include <string.h>

#include "mi2-input-stream.h"

G_DEFINE_TYPE (Mi2InputStream, mi2_input_stream, G_TYPE_DATA_INPUT_STREAM)

static void
mi2_input_stream_class_init (Mi2InputStreamClass *klass)
{
}

static void
mi2_input_stream_init (Mi2InputStream *self)
{
}

Mi2InputStream *
mi2_input_stream_new (GInputStream *base_stream)
{
  return g_object_new (MI2_TYPE_INPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

static void
mi2_input_stream_read_message_read_line_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  Mi2InputStream *self = (Mi2InputStream *)object;
  g_autoptr(Mi2Message) message = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autofree gchar *line = NULL;
  gsize len = 0;

  g_assert (MI2_IS_INPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  line = g_data_input_stream_read_line_finish_utf8 (G_DATA_INPUT_STREAM (self), result, &len, &error);

  /* Nothing to read, return NULL message */
  if (line == NULL && error == NULL)
    {
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  if (line != NULL && g_str_has_prefix (line, "(gdb)"))
    {
      /* Ignore this line, read again */
      GCancellable *cancellable = g_task_get_cancellable (task);
      g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                           G_PRIORITY_LOW,
                                           cancellable,
                                           mi2_input_stream_read_message_read_line_cb,
                                           g_steal_pointer (&task));
      return;
    }

  if (line == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "The stream has closed");
      return;
    }

  /* Let Mi2Message decode the protocol content */
  if (NULL == (message = mi2_message_parse (line, len, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&message), g_object_unref);
}

void
mi2_input_stream_read_message_async (Mi2InputStream      *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (MI2_IS_INPUT_STREAM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mi2_input_stream_read_message_async);
  g_task_set_return_on_cancel (task, TRUE);

  g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                       G_PRIORITY_LOW,
                                       cancellable,
                                       mi2_input_stream_read_message_read_line_cb,
                                       g_steal_pointer (&task));
}

/**
 * mi2_input_stream_read_message_finish:
 *
 * Finish an asynchronous call started by mi2_input_stream_read_message_async().
 *
 * Returns: (transfer full): An #Mi2Message if successful. If there is no data
 *   to read, this function returns %NULL and @error is not set. If an error
 *   occurred, %NULL is returned and @error is set.
 */
Mi2Message *
mi2_input_stream_read_message_finish (Mi2InputStream  *self,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (MI2_IS_INPUT_STREAM (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}
