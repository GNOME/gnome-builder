/* mi2-output-stream.c
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

#define G_LOD_DOMAIN "mi2-output-stream"

#include "mi2-output-stream.h"

G_DEFINE_TYPE (Mi2OutputStream, mi2_output_stream, G_TYPE_DATA_OUTPUT_STREAM)

static void
mi2_output_stream_class_init (Mi2OutputStreamClass *klass)
{
}

static void
mi2_output_stream_init (Mi2OutputStream *self)
{
}

Mi2OutputStream *
mi2_output_stream_new (GOutputStream *base_stream)
{
  return g_object_new (MI2_TYPE_OUTPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

static void
mi2_output_stream_write_message_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  Mi2OutputStream *self = (Mi2OutputStream *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (MI2_IS_OUTPUT_STREAM (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_output_stream_write_bytes_finish (G_OUTPUT_STREAM (self), result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

void
mi2_output_stream_write_message_async (Mi2OutputStream     *self,
                                       Mi2Message          *message,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_return_if_fail (MI2_IS_OUTPUT_STREAM (self));
  g_return_if_fail (MI2_IS_MESSAGE (message));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mi2_output_stream_write_message_async);

  bytes = mi2_message_serialize (message);

  if (bytes == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "message failed to serialize to bytes");
      return;
    }

  g_output_stream_write_bytes_async (G_OUTPUT_STREAM (self),
                                     bytes,
                                     G_PRIORITY_LOW,
                                     cancellable,
                                     mi2_output_stream_write_message_cb,
                                     g_steal_pointer (&task));
}

gboolean
mi2_output_stream_write_message_finish (Mi2OutputStream  *self,
                                        GAsyncResult     *result,
                                        GError          **error)
{
  g_return_val_if_fail (MI2_IS_OUTPUT_STREAM (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
