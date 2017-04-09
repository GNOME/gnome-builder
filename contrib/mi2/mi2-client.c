/* mi2-client.c
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

#define G_LOG_DOMAIN "mi2-client"

#include "mi2-client.h"
#include "mi2-command-message.h"
#include "mi2-console-message.h"
#include "mi2-error.h"
#include "mi2-event-message.h"
#include "mi2-input-stream.h"
#include "mi2-output-stream.h"
#include "mi2-reply-message.h"

typedef struct
{
  GIOStream       *io_stream;
  Mi2InputStream  *input_stream;
  Mi2OutputStream *output_stream;
  GCancellable    *read_loop_cancellable;
  GTask           *current_exec;

  guint            is_listening : 1;
} Mi2ClientPrivate;

enum {
  PROP_0,
  PROP_IO_STREAM,
  N_PROPS
};

enum {
  BREAKPOINT_INSERTED,
  BREAKPOINT_REMOVED,
  EVENT,
  LOG,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (Mi2Client, mi2_client, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
mi2_client_check_ready (Mi2Client  *self,
                        GError    **error)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);

  if (priv->input_stream == NULL || priv->output_stream == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_CONNECTED,
                   "Not connected to gdb");
      return FALSE;
    }

  return TRUE;
}

static void
mi2_client_set_io_stream (Mi2Client *self,
                          GIOStream *io_stream)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_assert (MI2_IS_CLIENT (self));
  g_assert (!io_stream || G_IS_IO_STREAM (io_stream));

  if (g_set_object (&priv->io_stream, io_stream))
    {
      priv->input_stream = mi2_input_stream_new (g_io_stream_get_input_stream (io_stream));
      priv->output_stream = mi2_output_stream_new (g_io_stream_get_output_stream (io_stream));
    }
}

static void
mi2_client_finalize (GObject *object)
{
  Mi2Client *self = (Mi2Client *)object;
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_clear_object (&priv->io_stream);
  g_clear_object (&priv->input_stream);
  g_clear_object (&priv->output_stream);
  g_clear_object (&priv->read_loop_cancellable);

  G_OBJECT_CLASS (mi2_client_parent_class)->finalize (object);
}

static void
mi2_client_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  Mi2Client *self = MI2_CLIENT (object);
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      g_value_set_object (value, priv->io_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_client_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  Mi2Client *self = MI2_CLIENT (object);

  switch (prop_id)
    {
    case PROP_IO_STREAM:
      mi2_client_set_io_stream (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_client_class_init (Mi2ClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_client_finalize;
  object_class->get_property = mi2_client_get_property;
  object_class->set_property = mi2_client_set_property;

  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The undelrying stream to communicate with",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [BREAKPOINT_INSERTED] =
    g_signal_new ("breakpoint-inserted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (Mi2ClientClass, breakpoint_inserted),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, MI2_TYPE_BREAKPOINT);

  signals [BREAKPOINT_REMOVED] =
    g_signal_new ("breakpoint-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (Mi2ClientClass, breakpoint_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  signals [EVENT] =
    g_signal_new ("event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (Mi2ClientClass, event),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, MI2_TYPE_EVENT_MESSAGE);

  signals [LOG] =
    g_signal_new ("log",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (Mi2ClientClass, log),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
mi2_client_init (Mi2Client *self)
{
}

Mi2Client *
mi2_client_new (GIOStream *io_stream)
{
  return g_object_new (MI2_TYPE_CLIENT,
                       "io-stream", io_stream,
                       NULL);
}

static void
mi2_client_exec_write_message_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  Mi2OutputStream *stream = (Mi2OutputStream *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (MI2_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!mi2_output_stream_write_message_finish (stream, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));

  /*
   * Do not successfully complete request here.
   *
   * Successful completion of the task must come from a reply
   * sent to us by the peer looking something like ^running.
   */
}

void
mi2_client_exec_async (Mi2Client           *self,
                       const gchar         *command,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);
  g_autoptr(Mi2Message) message = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (MI2_IS_CLIENT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mi2_client_exec_async);

  if (!mi2_client_check_ready (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (priv->current_exec != NULL)
    {
      g_task_return_new_error (task,
                               MI2_ERROR,
                               MI2_ERROR_EXEC_PENDING,
                               "An operation is already pending");
      return;
    }

  message = g_object_new (MI2_TYPE_COMMAND_MESSAGE,
                          "command", command,
                          NULL);

  priv->current_exec = g_object_ref (task);

  mi2_output_stream_write_message_async (priv->output_stream,
                                         message,
                                         cancellable,
                                         mi2_client_exec_write_message_cb,
                                         g_steal_pointer (&task));
}

/**
 * mi2_client_exec_finish:
 * @self: An #Mi2Client
 * @result: A #GAsyncResult
 * @reply: (optional) (out) (transfer full): A location for a reply.
 * @error: a location for a #GError or %NULL
 *
 * Completes a request to mi2_client_exec_async(). The reply from the
 * gdb instance will be provided to @message.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
mi2_client_exec_finish (Mi2Client        *self,
                        GAsyncResult     *result,
                        Mi2ReplyMessage **reply,
                        GError          **error)
{
  g_autoptr(Mi2ReplyMessage) local_message = NULL;
  gboolean ret;

  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_message = g_task_propagate_pointer (G_TASK (result), error);
  ret = !!local_message;

  if (reply)
    *reply = g_steal_pointer (&local_message);

  return ret;
}

static void
mi2_client_dispatch (Mi2Client  *self,
                     Mi2Message *message)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_return_if_fail (MI2_IS_CLIENT (self));
  g_return_if_fail (MI2_IS_MESSAGE (message));

  if (MI2_IS_CONSOLE_MESSAGE (message))
    {
      const gchar *str;

      str = mi2_console_message_get_message (MI2_CONSOLE_MESSAGE (message));
      g_signal_emit (self, signals [LOG], 0, str);
    }
  else if (MI2_IS_EVENT_MESSAGE (message))
    {
      const gchar *name = mi2_event_message_get_name (MI2_EVENT_MESSAGE (message));
      GQuark detail = g_quark_try_string (name);

      g_signal_emit (self, signals [EVENT], detail, message);
    }
  else if (MI2_IS_REPLY_MESSAGE (message))
    {
      g_autoptr(GTask) task = g_steal_pointer (&priv->current_exec);

      if (task != NULL)
        {
          g_autoptr(GError) error = NULL;

          if (mi2_reply_message_check_error (MI2_REPLY_MESSAGE (message), &error))
            g_task_return_error (task, g_steal_pointer (&error));
          else
            g_task_return_pointer (task, g_object_ref (message), g_object_unref);
        }
    }
  else
    g_print ("Got message of type %s\n", G_OBJECT_TYPE_NAME (message));
}

static void
mi2_client_read_loop_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  Mi2InputStream *stream = (Mi2InputStream *)object;
  g_autoptr(Mi2Client) self = user_data;
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);
  g_autoptr(Mi2Message) message = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (MI2_IS_INPUT_STREAM (stream));
  g_assert (MI2_IS_CLIENT (self));

  message = mi2_input_stream_read_message_finish (stream, result, &error);

  if (message == NULL)
    {
      priv->is_listening = FALSE;
      g_clear_object (&priv->read_loop_cancellable);
      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      return;
    }

  mi2_client_dispatch (self, message);

  if (priv->is_listening)
    mi2_input_stream_read_message_async (priv->input_stream,
                                         priv->read_loop_cancellable,
                                         mi2_client_read_loop_cb,
                                         g_steal_pointer (&self));
}

void
mi2_client_start_listening (Mi2Client *self)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_return_if_fail (MI2_IS_CLIENT (self));
  g_return_if_fail (priv->is_listening == FALSE);
  g_return_if_fail (priv->input_stream != NULL);

  priv->is_listening = TRUE;
  priv->read_loop_cancellable = g_cancellable_new ();

  mi2_input_stream_read_message_async (priv->input_stream,
                                       priv->read_loop_cancellable,
                                       mi2_client_read_loop_cb,
                                       g_object_ref (self));
}

void
mi2_client_stop_listening (Mi2Client *self)
{
  Mi2ClientPrivate *priv = mi2_client_get_instance_private (self);

  g_return_if_fail (MI2_IS_CLIENT (self));

  if (priv->is_listening)
    {
      priv->is_listening = FALSE;
      g_cancellable_cancel (priv->read_loop_cancellable);
    }
}

static void
mi2_client_insert_breakpoint_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  Mi2Client *self = (Mi2Client *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(Mi2ReplyMessage) message = NULL;
  Mi2Breakpoint *breakpoint;
  GVariant *bkpt;

  g_assert (MI2_IS_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!mi2_client_exec_finish (self, result, &message, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  breakpoint = g_task_get_task_data (task);

  bkpt = mi2_message_get_param (MI2_MESSAGE (message), "bkpt");

  if (bkpt != NULL)
    {
      GVariantDict dict;
      const gchar *number = NULL;

      g_variant_dict_init (&dict, bkpt);
      g_variant_dict_lookup (&dict, "number", "&s", &number);
      g_variant_dict_clear (&dict);

      mi2_breakpoint_set_id (breakpoint, g_ascii_strtoll (number, NULL, 10));
    }

  g_signal_emit (self, signals [BREAKPOINT_INSERTED], 0, breakpoint);

  g_task_return_boolean (task, TRUE);
}

/**
 * mi2_client_insert_breakpoint_async:
 * @self: A #Mi2Client
 * @breakpoint: An #Mi2Breakpoint
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A callback to execute
 * @user_data: user data for @callback
 *
 * Adds a breakpoint at @function. If @filename is specified, the function
 * will be resolved within that file.
 *
 * Call mi2_client_insert_breakpoint_async() to complete the operation.
 */
void
mi2_client_insert_breakpoint_async (Mi2Client           *self,
                                    Mi2Breakpoint       *breakpoint,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(Mi2CommandMessage) command = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GString) str = NULL;
  const gchar *address;
  const gchar *filename;
  const gchar *function;
  const gchar *linespec;
  gint line_offset;

  g_return_if_fail (MI2_IS_CLIENT (self));
  g_return_if_fail (MI2_IS_BREAKPOINT (breakpoint));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mi2_client_insert_breakpoint_async);
  g_task_set_task_data (task, g_object_ref (breakpoint), g_object_unref);

  str = g_string_new ("-break-insert");

  line_offset = mi2_breakpoint_get_line_offset (breakpoint);
  linespec = mi2_breakpoint_get_linespec (breakpoint);
  function = mi2_breakpoint_get_function (breakpoint);
  filename = mi2_breakpoint_get_filename (breakpoint);
  address = mi2_breakpoint_get_address (breakpoint);

  if (linespec)
    g_string_append_printf (str, " %s", linespec);

  if (filename)
    g_string_append_printf (str, " --source %s", filename);

  if (function)
    g_string_append_printf (str, " --function %s", function);

  if (line_offset)
    g_string_append_printf (str, " --line %d", line_offset);

  if (address)
    g_string_append_printf (str, " %s", address);

  command = g_object_new (MI2_TYPE_COMMAND_MESSAGE,
                          "command", str,
                          NULL);

  mi2_client_exec_async (self,
                         str->str,
                         cancellable,
                         mi2_client_insert_breakpoint_cb,
                         g_steal_pointer (&task));
}

gint
mi2_client_insert_breakpoint_finish (Mi2Client     *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
mi2_client_remove_breakpoint_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  Mi2Client *self = (Mi2Client *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  gint id;

  g_assert (MI2_IS_CLIENT (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!mi2_client_exec_finish (self, result, NULL, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  id = GPOINTER_TO_INT (g_task_get_task_data (task));
  g_signal_emit (self, signals [BREAKPOINT_REMOVED], 0, id);

  g_task_return_boolean (task, TRUE);
}

/**
 * mi2_client_remove_breakpoint_async:
 * @self: A #Mi2Client
 * @breakpoint_id: The id of the breakpoint
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A callback to execute
 * @user_data: user data for @callback
 *
 * Removes a breakpoint that was previously added.
 *
 * Call mi2_client_remove_breakpoint_finish() to complete the operation.
 */
void
mi2_client_remove_breakpoint_async (Mi2Client           *self,
                                    gint                 breakpoint_id,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(Mi2CommandMessage) command = NULL;
  g_autofree gchar *str = NULL;

  g_return_if_fail (MI2_IS_CLIENT (self));
  g_return_if_fail (breakpoint_id > 0);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, mi2_client_remove_breakpoint_async);
  g_task_set_task_data (task, GINT_TO_POINTER (breakpoint_id), NULL);

  str = g_strdup_printf ("-break-delete %d", breakpoint_id);

  command = g_object_new (MI2_TYPE_COMMAND_MESSAGE,
                          "command", str,
                          NULL);

  mi2_client_exec_async (self,
                         str,
                         cancellable,
                         mi2_client_remove_breakpoint_cb,
                         g_steal_pointer (&task));
}

gboolean
mi2_client_remove_breakpoint_finish (Mi2Client     *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

#if 0
void
mi2_client_async (Mi2Client           *self,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
}

gboolean
mi2_client_finish (Mi2Client     *self,
                   GAsyncResult  *result,
                   GError       **error)
{
  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
#endif
