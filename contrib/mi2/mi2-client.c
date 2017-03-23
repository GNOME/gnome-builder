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
#include "mi2-event-message.h"
#include "mi2-input-stream.h"
#include "mi2-output-stream.h"

typedef struct
{
  GIOStream       *io_stream;
  Mi2InputStream  *input_stream;
  Mi2OutputStream *output_stream;
  GCancellable    *read_loop_cancellable;

  guint is_listening : 1;
} Mi2ClientPrivate;

enum {
  PROP_0,
  PROP_IO_STREAM,
  N_PROPS
};

enum {
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
  else
    g_task_return_boolean (task, TRUE);
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

  message = g_object_new (MI2_TYPE_COMMAND_MESSAGE,
                          "command", command,
                          NULL);

  mi2_output_stream_write_message_async (priv->output_stream,
                                         message,
                                         cancellable,
                                         mi2_client_exec_write_message_cb,
                                         g_steal_pointer (&task));
}

gboolean
mi2_client_exec_finish (Mi2Client     *self,
                        GAsyncResult  *result,
                        GError       **error)
{
  g_return_val_if_fail (MI2_IS_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mi2_client_dispatch (Mi2Client  *self,
                     Mi2Message *message)
{
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
      g_print ("Event: %s\n", mi2_event_message_get_name (MI2_EVENT_MESSAGE (message)));
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
