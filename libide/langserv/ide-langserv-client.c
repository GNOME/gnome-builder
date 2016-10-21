/* ide-langserv-client.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-langserv-client"

#include <egg-signal-group.h>
#include <jsonrpc-glib.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "langserv/ide-langserv-client.h"

typedef struct
{
  EggSignalGroup *buffer_manager_signals;
  JsonrpcClient  *rpc_client;
  GIOStream      *io_stream;
} IdeLangservClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLangservClient, ide_langserv_client, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_IO_STREAM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static JsonNode *
create_text_document (IdeBuffer *buffer)
{
  g_autoptr(JsonNode) ret = NULL;
  g_autoptr(JsonObject) text_document = NULL;
  g_autofree gchar *uri = NULL;
  IdeFile *file;
  GFile *gfile;

  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (file);
  uri = g_file_get_uri (gfile);

  text_document = json_object_new ();
  json_object_set_string_member (text_document, "uri", uri);

  ret = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (ret, g_steal_pointer (&text_document));

  return g_steal_pointer (&ret);
}

static void
ide_langserv_client_buffer_loaded (IdeLangservClient *self,
                                   IdeBuffer         *buffer,
                                   IdeBufferManager  *buffer_manager)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) params = NULL;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  object = json_object_new ();
  json_object_set_member (object, "textDocument", create_text_document (buffer));

  params = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (params, g_steal_pointer (&object));

  jsonrpc_client_notification_async (priv->rpc_client, "textDocument/didOpen", params, NULL, NULL, NULL);
}

static void
ide_langserv_client_buffer_unloaded (IdeLangservClient *self,
                                     IdeBuffer         *buffer,
                                     IdeBufferManager  *buffer_manager)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(JsonNode) params = NULL;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  object = json_object_new ();
  json_object_set_member (object, "textDocument", create_text_document (buffer));

  params = json_node_new (JSON_NODE_OBJECT);
  json_node_set_object (params, g_steal_pointer (&object));

  jsonrpc_client_notification_async (priv->rpc_client, "textDocument/didClose", params, NULL, NULL, NULL);
}

static void
ide_langserv_client_buffer_manager_bind (IdeLangservClient *self,
                                         IdeBufferManager  *buffer_manager,
                                         EggSignalGroup    *signal_group)
{
  guint n_items;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (EGG_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_langserv_client_buffer_loaded (self, buffer, buffer_manager);
    }
}

static void
ide_langserv_client_buffer_manager_unbind (IdeLangservClient *self,
                                           EggSignalGroup    *signal_group)
{
  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (EGG_IS_SIGNAL_GROUP (signal_group));

  /* TODO: We need to track everything we've notified so that we
   *       can notify the peer to release its resources.
   */
}

static void
ide_langserv_client_finalize (GObject *object)
{
  IdeLangservClient *self = (IdeLangservClient *)object;
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  g_clear_object (&priv->rpc_client);
  g_clear_object (&priv->buffer_manager_signals);

  G_OBJECT_CLASS (ide_langserv_client_parent_class)->finalize (object);
}

static void
ide_langserv_client_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeLangservClient *self = IDE_LANGSERV_CLIENT (object);
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

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
ide_langserv_client_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeLangservClient *self = IDE_LANGSERV_CLIENT (object);
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

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
ide_langserv_client_class_init (IdeLangservClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_client_finalize;
  object_class->get_property = ide_langserv_client_get_property;
  object_class->set_property = ide_langserv_client_set_property;

  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The GIOStream to communicate over",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_langserv_client_init (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  priv->buffer_manager_signals = egg_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  egg_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-loaded",
                                   G_CALLBACK (ide_langserv_client_buffer_loaded),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-unloaded",
                                   G_CALLBACK (ide_langserv_client_buffer_unloaded),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->buffer_manager_signals,
                           "bind",
                           G_CALLBACK (ide_langserv_client_buffer_manager_bind),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->buffer_manager_signals,
                           "unbind",
                           G_CALLBACK (ide_langserv_client_buffer_manager_unbind),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_langserv_client_notification (IdeLangservClient *self,
                                  const gchar       *method,
                                  JsonNode          *params,
                                  JsonrpcClient     *rpc_client)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (method != NULL);
  g_assert (params != NULL);
  g_assert (rpc_client != NULL);

  IDE_TRACE_MSG ("Notification: %s", method);

  IDE_EXIT;
}

IdeLangservClient *
ide_langserv_client_new (IdeContext *context,
                         GIOStream  *io_stream)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_LANGSERV_CLIENT,
                       "context", context,
                       "io-stream", io_stream,
                       NULL);
}

void
ide_langserv_client_start (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (G_IS_IO_STREAM (priv->io_stream) && IDE_IS_CONTEXT (context))
    {
      IdeBufferManager *buffer_manager = NULL;

      priv->rpc_client = jsonrpc_client_new (priv->io_stream);

      g_signal_connect_object (priv->rpc_client,
                               "notification",
                               G_CALLBACK (ide_langserv_client_notification),
                               self,
                               G_CONNECT_SWAPPED);

      /*
       * The first thing we need to do is initialize the client with information
       * about our project. So that we will perform asynchronously here. It will
       * also start our read loop.
       */

      buffer_manager = ide_context_get_buffer_manager (context);
      egg_signal_group_set_target (priv->buffer_manager_signals, buffer_manager);
    }
  else
    {
      g_warning ("Cannot start %s due to misconfiguration.",
                 G_OBJECT_TYPE_NAME (self));
    }

  IDE_EXIT;
}

void
ide_langserv_client_stop (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));

  if (priv->rpc_client != NULL)
    {
      jsonrpc_client_close_async (priv->rpc_client, NULL, NULL, NULL);
      g_clear_object (&priv->rpc_client);
    }

  IDE_EXIT;
}
