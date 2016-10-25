/* jsonrpc-client.c
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

#define G_LOG_DOMAIN "jsonrpc-client"

/**
 * SECTION:jsonrpc-client:
 * @title: JsonrpcClient
 * @short_description: a client for JSON-RPC communication
 *
 * The #JsonrpcClient class provides a convenient API to coordinate with a
 * JSON-RPC server. You can provide the underlying #GIOStream to communicate
 * with allowing you to control the negotiation of how you setup your
 * communications channel. One such method might be to use a #GSubprocess and
 * communicate over stdin and stdout.
 *
 * Because JSON-RPC allows for out-of-band notifications from the server to
 * the client, it is important that the consumer of this API calls
 * jsonrpc_client_close() or jsonrpc_client_close_async() when they no longer
 * need the client. This is because #JsonrpcClient contains an asynchronous
 * read-loop to process incoming messages. Until jsonrpc_client_close() or
 * jsonrpc_client_close_async() have been called, this read loop will prevent
 * the object from finalizing (being freed).
 *
 * To make an RPC call, use jsonrpc_client_call() or
 * jsonrpc_client_call_async() and provide the method name and the parameters
 * as a #JsonNode for call.
 *
 * It is a programming error to mix synchronous and asynchronous API calls
 * of the #JsonrpcClient class.
 *
 * For synchronous calls, #JsonrpcClient will use the thread-default
 * #GMainContext. If you have special needs here ensure you've set the context
 * before calling into any #JsonrpcClient API.
 */

#include <glib.h>

#include "jsonrpc-client.h"
#include "jsonrpc-input-stream.h"
#include "jsonrpc-output-stream.h"

typedef struct
{
  /*
   * The invocations field contains a hashtable that maps request ids to
   * the GTask that is awaiting their completion. The tasks are removed
   * from the hashtable automatically upon completion by connecting to
   * the GTask::completed signal. When reading a message from the input
   * stream, we use the request id as a string to lookup the inflight
   * invocation. The result is passed as the result of the task.
   */
  GHashTable *invocations;

  /*
   * We hold an extra reference to the GIOStream pair to make things
   * easier to construct and ensure that the streams are in tact in
   * case they are poorly implemented.
   */
  GIOStream *io_stream;

  /*
   * The input_stream field contains our wrapper input stream around the
   * underlying input stream provided by JsonrpcClient::io-stream. This
   * allows us to conveniently write JsonNode instances.
   */
  JsonrpcInputStream *input_stream;

  /*
   * The output_stream field contains our wrapper output stream around the
   * underlying output stream provided by JsonrpcClient::io-stream. This
   * allows us to convieniently read JsonNode instances.
   */
  JsonrpcOutputStream *output_stream;

  /*
   * This cancellable is used for our async read loops so that we can
   * cancel the operation to shutdown the client. Otherwise, we would
   * indefinitely leak our client due to the self-reference on our
   * read loop user_data parameter.
   */
  GCancellable *read_loop_cancellable;

  /*
   * Every JSONRPC invocation needs a request id. This is a monotonic
   * integer that we encode as a string to the server.
   */
  gint sequence;

  /*
   * This bit indicates if we have sent a call yet. Once we send our
   * first call, we start our read loop which will allow us to also
   * dispatch notifications out of band.
   */
  guint is_first_call : 1;

  /*
   * This bit is set when the program has called jsonrpc_client_close()
   * or jsonrpc_client_close_async(). When the read loop returns, it
   * will check for this and discontinue further asynchronous reads.
   */
  guint in_shutdown : 1;

  /*
   * If we have panic'd, this will be set to TRUE so that we can short
   * circuit on future operations sooner.
   */
  guint failed : 1;
} JsonrpcClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcClient, jsonrpc_client, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_IO_STREAM,
  N_PROPS
};

enum {
  NOTIFICATION,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

/*
 * Check to see if this looks like a jsonrpc 2.0 reply of any kind.
 */
static gboolean
is_jsonrpc_reply (JsonNode *node)
{
  JsonObject *object;
  const gchar *value;

  return JSON_NODE_HOLDS_OBJECT (node) &&
         NULL != (object = json_node_get_object (node)) &&
         json_object_has_member (object, "jsonrpc") &&
         NULL != (value = json_object_get_string_member (object, "jsonrpc")) &&
         (g_strcmp0 (value, "2.0") == 0);
}

/*
 * Check to see if this looks like a notification reply.
 */
static gboolean
is_jsonrpc_notification (JsonNode *node)
{
  JsonObject *object;
  const gchar *value;

  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);

  return !json_object_has_member (object, "id") &&
         json_object_has_member (object, "method") &&
         NULL != (value = json_object_get_string_member (object, "method")) &&
         value != NULL && *value != '\0';
}

/*
 * Check to see if this looks like a proper result for an RPC.
 */
static gboolean
is_jsonrpc_result (JsonNode *node)
{
  JsonObject *object;
  JsonNode *field;

  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);

  return json_object_has_member (object, "id") &&
         NULL != (field = json_object_get_member (object, "id")) &&
         JSON_NODE_HOLDS_VALUE (field) &&
         json_node_get_int (field) > 0 &&
         json_object_has_member (object, "result");
}

/*
 * Try to unwrap the error and possibly set @id to the extracted RPC
 * request id.
 */
static gboolean
unwrap_jsonrpc_error (JsonNode  *node,
                      gint      *id,
                      GError   **error)
{
  JsonObject *object;
  JsonObject *err_obj;
  JsonNode *field;

  g_assert (node != NULL);
  g_assert (id != NULL);
  g_assert (error != NULL);

  if (!JSON_NODE_HOLDS_OBJECT (node))
    return FALSE;

  object = json_node_get_object (node);

  if (json_object_has_member (object, "id") &&
      NULL != (field = json_object_get_member (object, "id")) &&
      JSON_NODE_HOLDS_VALUE (field) &&
      json_node_get_int (field) > 0)
    *id = json_node_get_int (field);
  else
    *id = -1;

  if (json_object_has_member (object, "error") &&
      NULL != (field = json_object_get_member (object, "error")) &&
      JSON_NODE_HOLDS_OBJECT (field) &&
      NULL != (err_obj = json_node_get_object (field)))
    {
      const gchar *message;
      gint code;

      message = json_object_get_string_member (err_obj, "message");
      code = json_object_get_int_member (err_obj, "code");

      if (message == NULL || *message == '\0')
        message = "Unknown error occurred";

      g_set_error_literal (error, JSONRPC_CLIENT_ERROR, code, message);

      return TRUE;
    }

  return FALSE;
}

/*
 * jsonrpc_client_panic:
 *
 * This function should be called to "tear down everything" and ensure we
 * cleanup.
 */
static void
jsonrpc_client_panic (JsonrpcClient *self,
                      const GError  *error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GHashTable) invocations = NULL;
  GHashTableIter iter;
  GTask *task;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (error != NULL);

  priv->failed = TRUE;

  g_warning ("%s", error->message);

  jsonrpc_client_close (self, NULL, NULL);

  /* Steal the tasks so that we don't have to worry about reentry. */
  invocations = g_steal_pointer (&priv->invocations);
  priv->invocations = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  /*
   * Clear our input and output streams so that new calls
   * fail immediately due to not being connected.
   */
  g_clear_object (&priv->input_stream);
  g_clear_object (&priv->output_stream);

  /*
   * Now notify all of the in-flight invocations that they failed due
   * to an unrecoverable error.
   */
  g_hash_table_iter_init (&iter, invocations);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&task))
    g_task_return_error (task, g_error_copy (error));
}

/*
 * jsonrpc_client_check_ready:
 *
 * Checks to see if the client is in a position to make requests.
 *
 * Returns: %TRUE if the client is ready for RPCs; otherwise %FALSE
 *   and @error is set.
 */
static gboolean
jsonrpc_client_check_ready (JsonrpcClient  *self,
                            GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_assert (JSONRPC_IS_CLIENT (self));

  if (priv->failed || priv->in_shutdown || priv->output_stream == NULL || priv->input_stream == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "No stream available to deliver invocation");
      return FALSE;
    }

  return TRUE;
}

static void
jsonrpc_client_constructed (GObject *object)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  GInputStream *input_stream;
  GOutputStream *output_stream;

  G_OBJECT_CLASS (jsonrpc_client_parent_class)->constructed (object);

  if (priv->io_stream == NULL)
    {
      g_warning ("%s requires a GIOStream to communicate. Disabling.",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  input_stream = g_io_stream_get_input_stream (priv->io_stream);
  output_stream = g_io_stream_get_output_stream (priv->io_stream);

  priv->input_stream = jsonrpc_input_stream_new (input_stream);
  priv->output_stream = jsonrpc_output_stream_new (output_stream);
}

static void
jsonrpc_client_finalize (GObject *object)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  g_clear_pointer (&priv->invocations, g_hash_table_unref);

  g_clear_object (&priv->input_stream);
  g_clear_object (&priv->output_stream);
  g_clear_object (&priv->io_stream);
  g_clear_object (&priv->read_loop_cancellable);

  G_OBJECT_CLASS (jsonrpc_client_parent_class)->finalize (object);
}

static void
jsonrpc_client_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  JsonrpcClient *self = JSONRPC_CLIENT (object);
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      priv->io_stream = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
jsonrpc_client_class_init (JsonrpcClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = jsonrpc_client_constructed;
  object_class->finalize = jsonrpc_client_finalize;
  object_class->set_property = jsonrpc_client_set_property;

  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The stream to communicate over",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcClientClass, notification),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  JSON_TYPE_NODE);
}

static void
jsonrpc_client_init (JsonrpcClient *self)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);

  priv->invocations = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  priv->is_first_call = TRUE;
  priv->read_loop_cancellable = g_cancellable_new ();
}

/**
 * jsonrpc_client_new:
 * @io_stream: A #GIOStream
 *
 * Creates a new #JsonrpcClient instance.
 *
 * If you want to communicate with a process using stdin/stdout, consider using
 * #GSubprocess to launch the process and create a #GSimpleIOStream using the
 * g_subprocess_get_stdin_pipe() and g_subprocess_get_stdout_pipe().
 *
 * Returns: (transfer full): A newly created #JsonrpcClient
 */
JsonrpcClient *
jsonrpc_client_new (GIOStream *io_stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (io_stream), NULL);

  return g_object_new (JSONRPC_TYPE_CLIENT,
                       "io-stream", io_stream,
                       NULL);
}

static void
jsonrpc_client_call_notify_completed (GTask      *task,
                                      GParamSpec *pspec,
                                      gpointer    user_data)
{
  JsonrpcClientPrivate *priv;
  JsonrpcClient *self;
  gpointer id;

  g_assert (G_IS_TASK (task));
  g_assert (pspec != NULL);
  g_assert (g_str_equal (pspec->name, "completed"));

  self = g_task_get_source_object (task);
  priv = jsonrpc_client_get_instance_private (self);
  id = g_task_get_task_data (task);

  g_hash_table_remove (priv->invocations, id);
}

static void
jsonrpc_client_call_write_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* We don't need to complete the task because it will get completed when the
   * server replies with our reply. This is performed using an asynchronous
   * read that will pump through the messages.
   */
}

static void
jsonrpc_client_call_read_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  JsonrpcInputStream *stream = (JsonrpcInputStream *)object;
  g_autoptr(JsonrpcClient) self = user_data;
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  gint id = -1;

  g_assert (JSONRPC_IS_INPUT_STREAM (stream));
  g_assert (JSONRPC_IS_CLIENT (self));

  if (!jsonrpc_input_stream_read_message_finish (stream, result, &node, &error))
    {
      /*
       * Handle jsonrpc_client_close() conditions gracefully.
       */
      if (priv->in_shutdown && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      /*
       * If we fail to read a message, that means we couldn't even receive
       * a message describing the error. All we can do in this case is panic
       * and shutdown the whole client.
       */
      jsonrpc_client_panic (self, error);
      return;
    }

  g_assert (node != NULL);

  /*
   * If the message is malformed, we'll also need to perform another read.
   * We do this to try to be relaxed against failures. That seems to be
   * the JSONRPC way, although I'm not sure I like the idea.
   */
  if (!is_jsonrpc_reply (node))
    {
      error = g_error_new_literal (G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Received malformed response from peer");
      jsonrpc_client_panic (self, error);
      return;
    }

  /*
   * If the response does not have an "id" field, then it is a "notification"
   * and we need to emit the "notificiation" signal.
   */
  if (is_jsonrpc_notification (node))
    {
      g_autoptr(JsonNode) empty_params = NULL;
      const gchar *method_name;
      JsonObject *obj;
      JsonNode *params;

      obj = json_node_get_object (node);
      method_name = json_object_get_string_member (obj, "method");
      params = json_object_get_member (obj, "params");

      if (params == NULL)
        params = empty_params = json_node_new (JSON_NODE_ARRAY);

      g_signal_emit (self, signals [NOTIFICATION], 0, method_name, params);

      goto begin_next_read;
    }

  if (is_jsonrpc_result (node))
    {
      JsonObject *obj;
      JsonNode *res;
      GTask *task;

      obj = json_node_get_object (node);
      id = json_object_get_int_member (obj, "id");
      res = json_object_get_member (obj, "result");

      task = g_hash_table_lookup (priv->invocations, GINT_TO_POINTER (id));

      if (task != NULL)
        {
          g_task_return_pointer (task, json_node_copy (res), (GDestroyNotify)json_node_unref);
          goto begin_next_read;
        }

      error = g_error_new_literal (G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Reply to missing or invalid task");
      jsonrpc_client_panic (self, error);
      return;
    }

  /*
   * If we got an error destined for one of our inflight invocations, then
   * we need to dispatch it now.
   */
  if (unwrap_jsonrpc_error (node, &id, &error))
    {
      if (id > 0)
        {
          GTask *task = g_hash_table_lookup (priv->invocations, GINT_TO_POINTER (id));

          if (task != NULL)
            {
              g_task_return_error (task, g_steal_pointer (&error));
              goto begin_next_read;
            }
        }

      /*
       * Generic error, not tied to any specific task we had in flight. So
       * take this as a failure case and panic on the line.
       */
      jsonrpc_client_panic (self, error);
      return;
    }

  {
    g_autofree gchar *str = json_to_string (node, FALSE);
    g_warning ("Unhandled message: %s", str);
  }

begin_next_read:
  if (priv->input_stream != NULL && priv->in_shutdown == FALSE)
    jsonrpc_input_stream_read_message_async (priv->input_stream,
                                             priv->read_loop_cancellable,
                                             jsonrpc_client_call_read_cb,
                                             g_steal_pointer (&self));
}

static void
jsonrpc_client_call_sync_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  JsonrpcClient *self = (JsonrpcClient *)object;
  GTask *task = user_data;
  g_autoptr(JsonNode) return_value = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_client_call_finish (self, result, &return_value, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&return_value), (GDestroyNotify)json_node_unref);
}

/**
 * jsonrpc_client_call:
 * @self: A #JsonrpcClient
 * @method: the name of the method to call
 * @params: (transfer full) (nullable): A #JsonNode of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @return_value: (nullable) (out): A location for a #JsonNode.
 *
 * Synchronously calls @method with @params on the remote peer.
 *
 * This function takes ownership of @params.
 *
 * once a reply has been received, or failure, this function will return.
 * If successful, @return_value will be set with the reslut field of
 * the response.
 *
 * Returns; %TRUE on success; otherwise %FALSE and @error is set.
 */
gboolean
jsonrpc_client_call (JsonrpcClient  *self,
                     const gchar    *method,
                     JsonNode       *params,
                     GCancellable   *cancellable,
                     JsonNode      **return_value,
                     GError        **error)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GMainContext) main_context = NULL;
  g_autoptr(JsonNode) local_return_value = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  main_context = g_main_context_ref_thread_default ();

  task = g_task_new (NULL, NULL, NULL, NULL);
  g_task_set_source_tag (task, jsonrpc_client_call);

  jsonrpc_client_call_async (self,
                             method,
                             params,
                             cancellable,
                             jsonrpc_client_call_sync_cb,
                             task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (main_context, TRUE);

  local_return_value = g_task_propagate_pointer (task, error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  return ret;
}

/**
 * jsonrpc_client_call_async:
 * @self: A #JsonrpcClient
 * @method: the name of the method to call
 * @params: (transfer full) (nullable): A #JsonNode of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: a callback to executed upon completion
 * @user_data: user data for @callback
 *
 * Asynchronously calls @method with @params on the remote peer.
 *
 * This function takes ownership of @params.
 *
 * Upon completion or failure, @callback is executed and it should
 * call jsonrpc_client_call_finish() to complete the request and release
 * any memory held.
 */
void
jsonrpc_client_call_async (JsonrpcClient       *self,
                           const gchar         *method,
                           JsonNode            *params,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  gint id;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_call_async);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_signal_connect (task,
                    "notify::completed",
                    G_CALLBACK (jsonrpc_client_call_notify_completed),
                    NULL);

  id = ++priv->sequence;

  g_task_set_task_data (task, GINT_TO_POINTER (id), NULL);

  if (params == NULL)
    params = json_node_new (JSON_NODE_NULL);

  object = json_object_new ();

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_int_member (object, "id", id);
  json_object_set_string_member (object, "method", method);
  json_object_set_member (object, "params", params);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, g_steal_pointer (&object));

  g_hash_table_insert (priv->invocations, GINT_TO_POINTER (id), g_object_ref (task));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             node,
                                             cancellable,
                                             jsonrpc_client_call_write_cb,
                                             g_steal_pointer (&task));

  /*
   * If this is our very first message, then we need to start our
   * async read loop. This will allow us to receive notifications
   * out-of-band and intermixed with RPC calls.
   */

  if (priv->is_first_call)
    {
      priv->is_first_call = FALSE;

      /*
       * Because we take a reference here in our read loop, it is important
       * that the user calls jsonrpc_client_close() or
       * jsonrpc_client_close_async() so that we can cancel the operation and
       * allow it to cleanup any outstanding references.
       */
      jsonrpc_input_stream_read_message_async (priv->input_stream,
                                               priv->read_loop_cancellable,
                                               jsonrpc_client_call_read_cb,
                                               g_object_ref (self));
    }
}

/**
 * jsonrpc_client_call_finish:
 * @self: A #JsonrpcClient.
 * @result: A #GAsyncResult provided to the callback in jsonrpc_client_call_async()
 * @return_value: (out) (nullable): A location for a #JsonNode or %NULL
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous call to jsonrpc_client_call_async().
 *
 * Returns: %TRUE if successful and @return_value is set, otherwise %FALSE and @error is set.
 */
gboolean
jsonrpc_client_call_finish (JsonrpcClient  *self,
                            GAsyncResult   *result,
                            JsonNode      **return_value,
                            GError        **error)
{
  g_autoptr(JsonNode) local_return_value = NULL;
  gboolean ret;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_return_value = g_task_propagate_pointer (G_TASK (result), error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  return ret;
}

GQuark
jsonrpc_client_error_quark (void)
{
  return g_quark_from_static_string ("jsonrpc-client-error-quark");
}

static void
jsonrpc_client_notification_write_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  JsonrpcOutputStream *stream = (JsonrpcOutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!jsonrpc_output_stream_write_message_finish (stream, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_client_notification:
 * @self: A #JsonrpcClient
 * @method: the name of the method to call
 * @params: (transfer full) (nullable): A #JsonNode of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 *
 * Synchronously calls @method with @params on the remote peer.
 * This function will not wait or expect a reply from the peer.
 *
 * This function takes ownership of @params.
 *
 * Returns; %TRUE on success; otherwise %FALSE and @error is set.
 */
gboolean
jsonrpc_client_notification (JsonrpcClient  *self,
                             const gchar    *method,
                             JsonNode       *params,
                             GCancellable   *cancellable,
                             GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (method != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!jsonrpc_client_check_ready (self, error))
    return FALSE;

  if (params == NULL)
    params = json_node_new (JSON_NODE_NULL);

  object = json_object_new ();

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_string_member (object, "method", method);
  json_object_set_member (object, "params", params);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, g_steal_pointer (&object));

  return jsonrpc_output_stream_write_message (priv->output_stream, node, cancellable, error);
}

/**
 * jsonrpc_client_notification_async:
 * @self: A #JsonrpcClient
 * @method: the name of the method to call
 * @params: (transfer full) (nullable): A #JsonNode of parameters or %NULL
 * @cancellable: (nullable): A #GCancellable or %NULL
 *
 * Asynchronously calls @method with @params on the remote peer.
 * This function will not wait or expect a reply from the peer.
 *
 * This function is useful when the caller wants to be notified that
 * the bytes have been delivered to the underlying stream. This does
 * not indicate that the peer has received them.
 *
 * This function takes ownership of @params.
 */
void
jsonrpc_client_notification_async (JsonrpcClient       *self,
                                   const gchar         *method,
                                   JsonNode            *params,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_notification_async);

  if (!jsonrpc_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (params == NULL)
    params = json_node_new (JSON_NODE_NULL);

  object = json_object_new ();

  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_string_member (object, "method", method);
  json_object_set_member (object, "params", params);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, g_steal_pointer (&object));

  jsonrpc_output_stream_write_message_async (priv->output_stream,
                                             node,
                                             cancellable,
                                             jsonrpc_client_notification_write_cb,
                                             g_steal_pointer (&task));
}

/**
 * jsonrpc_client_notification_finish:
 * @self: A #JsonrpcClient
 *
 * Completes an asynchronous call to jsonrpc_client_notification_async().
 *
 * Successful completion of this function only indicates that the request
 * has been written to the underlying buffer, not that the peer has received
 * the notification.
 *
 * Returns: %TRUE if the bytes have been flushed to the #GIOStream; otherwise
 *   %FALSE and @error is set.
 */
gboolean
jsonrpc_client_notification_finish (JsonrpcClient  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * jsonrpc_client_close:
 * @self: A #JsonrpcClient
 *
 * Closes the underlying streams and cancels any inflight operations of the
 * #JsonrpcClient. This is important to call when you are done with the
 * client so that any outstanding operations that have caused @self to
 * hold additional references are cancelled.
 *
 * Failure to call this method results in a leak of #JsonrpcClient.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
jsonrpc_client_close (JsonrpcClient  *self,
                      GCancellable   *cancellable,
                      GError        **error)
{
  JsonrpcClientPrivate *priv = jsonrpc_client_get_instance_private (self);
  g_autoptr(GHashTable) invocations = NULL;
  g_autoptr(GError) local_error = NULL;
  GHashTableIter iter;
  GTask *task;

  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!jsonrpc_client_check_ready (self, error))
    return FALSE;

  priv->in_shutdown = TRUE;

  if (!g_cancellable_is_cancelled (priv->read_loop_cancellable))
    g_cancellable_cancel (priv->read_loop_cancellable);

  if (!g_output_stream_is_closed (G_OUTPUT_STREAM (priv->output_stream)))
    {
      if (!g_output_stream_close (G_OUTPUT_STREAM (priv->output_stream), cancellable, error))
        return FALSE;
    }

  if (!g_input_stream_is_closed (G_INPUT_STREAM (priv->input_stream)))
    {
      if (!g_input_stream_close (G_INPUT_STREAM (priv->input_stream), cancellable, error))
        return FALSE;
    }

  invocations = g_steal_pointer (&priv->invocations);
  priv->invocations = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  local_error = g_error_new_literal (G_IO_ERROR,
                                     G_IO_ERROR_CLOSED,
                                     "The underlying stream was closed");

  g_hash_table_iter_init (&iter, invocations);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&task))
    g_task_return_error (task, g_error_copy (local_error));

  return TRUE;
}

/**
 * jsonrpc_client_close_async:
 * @self: A #JsonrpcClient.
 *
 * Asynchronous version of jsonrpc_client_close()
 *
 * Currently this operation is implemented synchronously, but in the future may
 * be converted to using asynchronous operations.
 */
void
jsonrpc_client_close_async (JsonrpcClient       *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (JSONRPC_IS_CLIENT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, jsonrpc_client_close_async);

  /*
   * In practice, none of our close operations should block (unless they were
   * a FUSE fd or something like that. So we'll just perform them synchronously
   * for now.
   */
  jsonrpc_client_close (self, cancellable, NULL);

  g_task_return_boolean (task, TRUE);
}

/**
 * jsonrpc_client_close_finish:
 * @self A #JsonrpcClient.
 *
 * Completes an asynchronous request of jsonrpc_client_close_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
jsonrpc_client_close_finish (JsonrpcClient  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_return_val_if_fail (JSONRPC_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
