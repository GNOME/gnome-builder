/* jsonrpc-input-stream.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "jsonrpc-input-stream"

#include <errno.h>
#include <string.h>

#include "jsonrpc-input-stream.h"

typedef struct
{
  gssize content_length;
  gchar *buffer;
  gint priority;
} ReadState;

typedef struct
{
  gssize max_size_bytes;
} JsonrpcInputStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcInputStream, jsonrpc_input_stream, G_TYPE_DATA_INPUT_STREAM)

static gboolean jsonrpc_input_stream_debug;

static void
read_state_free (gpointer data)
{
  ReadState *state = data;

  g_free (state->buffer);
  g_slice_free (ReadState, state);
}

static void
jsonrpc_input_stream_class_init (JsonrpcInputStreamClass *klass)
{
  jsonrpc_input_stream_debug = !!g_getenv ("JSONRPC_DEBUG");
}

static void
jsonrpc_input_stream_init (JsonrpcInputStream *self)
{
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);

  /* 16 MB */
  priv->max_size_bytes = 16 * 1024 * 1024;

  g_data_input_stream_set_newline_type (G_DATA_INPUT_STREAM (self),
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);
}

JsonrpcInputStream *
jsonrpc_input_stream_new (GInputStream *base_stream)
{
  return g_object_new (JSONRPC_TYPE_INPUT_STREAM,
                       "base-stream", base_stream,
                       NULL);
}

static void
jsonrpc_input_stream_read_body_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  ReadState *state;
  JsonNode *root;
  gsize n_read;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  if (!g_input_stream_read_all_finish (G_INPUT_STREAM (self), result, &n_read, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if ((gssize)n_read != state->content_length)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Failed to read %"G_GSSIZE_FORMAT" bytes",
                               state->content_length);
      return;
    }

  state->buffer [state->content_length] = '\0';

  if G_UNLIKELY (jsonrpc_input_stream_debug)
    g_message ("<<< %s", state->buffer);

  parser = json_parser_new_immutable ();

  if (!json_parser_load_from_data (parser, state->buffer, state->content_length, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (NULL == (root = json_parser_get_root (parser)))
    {
      /*
       * If we get back a NULL root node, that means that we got
       * a short read (such as a closed stream).
       */
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "The peer did not send a reply");
      return;
    }

  g_task_return_pointer (task, json_node_copy (root), (GDestroyNotify)json_node_unref);
}

static void
jsonrpc_input_stream_read_headers_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  JsonrpcInputStreamPrivate *priv = jsonrpc_input_stream_get_instance_private (self);
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *line = NULL;
  GCancellable *cancellable = NULL;
  ReadState *state;
  gsize length = 0;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  line = g_data_input_stream_read_line_finish_utf8 (G_DATA_INPUT_STREAM (self), result, &length, &error);

  if (line == NULL)
    {
      if (error != NULL)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CLOSED,
                                 "The peer has closed the stream");
      return;
    }

  state = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  if (strncasecmp ("Content-Length: ", line, 16) == 0)
    {
      const gchar *lenptr = line + 16;
      gint64 content_length;

      content_length = g_ascii_strtoll (lenptr, NULL, 10);

      if (((content_length == G_MININT64 || content_length == G_MAXINT64) && errno == ERANGE) ||
          (content_length < 0) ||
          (content_length > G_MAXSSIZE) ||
          (content_length > priv->max_size_bytes))
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid Content-Length received from peer");
          return;
        }

      state->content_length = content_length;
    }

  /*
   * If we are at the end of the headers, we can make progress towards
   * parsing the JSON content. Otherwise we need to continue parsing
   * the next header.
   */

  if (line[0] == '\0')
    {
      if (state->content_length <= 0)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid or missing Content-Length header from peer");
          return;
        }

      state->buffer = g_malloc (state->content_length + 1);
      g_input_stream_read_all_async (G_INPUT_STREAM (self),
                                     state->buffer,
                                     state->content_length,
                                     state->priority,
                                     cancellable,
                                     jsonrpc_input_stream_read_body_cb,
                                     g_steal_pointer (&task));
      return;
    }

  g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                       state->priority,
                                       cancellable,
                                       jsonrpc_input_stream_read_headers_cb,
                                       g_steal_pointer (&task));
}

void
jsonrpc_input_stream_read_message_async (JsonrpcInputStream  *self,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  ReadState *state;

  g_return_if_fail (JSONRPC_IS_INPUT_STREAM (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (ReadState);
  state->content_length = -1;
  state->priority = G_PRIORITY_DEFAULT;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_input_stream_read_message_async);
  g_task_set_task_data (task, state, read_state_free);

  g_data_input_stream_read_line_async (G_DATA_INPUT_STREAM (self),
                                       state->priority,
                                       cancellable,
                                       jsonrpc_input_stream_read_headers_cb,
                                       g_steal_pointer (&task));
}

gboolean
jsonrpc_input_stream_read_message_finish (JsonrpcInputStream  *self,
                                          GAsyncResult        *result,
                                          JsonNode           **node,
                                          GError             **error)
{
  g_autoptr(JsonNode) local_node = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_INPUT_STREAM (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_node = g_task_propagate_pointer (G_TASK (result), error);
  ret = local_node != NULL;

  if (node != NULL)
    *node = g_steal_pointer (&local_node);

  return ret;
}

static void
jsonrpc_input_stream_read_message_sync_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  JsonrpcInputStream *self = (JsonrpcInputStream *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonNode) node = NULL;
  GTask *task = user_data;

  g_assert (JSONRPC_IS_INPUT_STREAM (self));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_input_stream_read_message_finish (self, result, &node, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&node), (GDestroyNotify)json_node_unref);
}

gboolean
jsonrpc_input_stream_read_message (JsonrpcInputStream  *self,
                                   GCancellable        *cancellable,
                                   JsonNode           **node,
                                   GError             **error)
{
  g_autoptr(GMainContext) main_context = NULL;
  g_autoptr(JsonNode) local_node = NULL;
  g_autoptr(GTask) task = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_INPUT_STREAM (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  main_context = g_main_context_ref_thread_default ();

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_source_tag (task, jsonrpc_input_stream_read_message);

  jsonrpc_input_stream_read_message_async (self,
                                           cancellable,
                                           jsonrpc_input_stream_read_message_sync_cb,
                                           task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (main_context, TRUE);

  local_node = g_task_propagate_pointer (task, error);
  ret = local_node != NULL;

  if (node != NULL)
    *node = g_steal_pointer (&local_node);

  return ret;
}

