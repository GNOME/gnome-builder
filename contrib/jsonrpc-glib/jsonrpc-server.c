/* jsonrpc-server.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
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

#define G_LOG_DOMAIN "jsonrpc-server"

#include "jsonrpc-input-stream.h"
#include "jsonrpc-output-stream.h"
#include "jsonrpc-server.h"

typedef struct
{
  GHashTable *clients;
} JsonrpcServerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (JsonrpcServer, jsonrpc_server, G_TYPE_OBJECT)

enum {
  HANDLE_CALL,
  NOTIFICATION,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
jsonrpc_server_finalize (GObject *object)
{
  JsonrpcServer *self = (JsonrpcServer *)object;
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  g_clear_pointer (&priv->clients, g_hash_table_unref);

  G_OBJECT_CLASS (jsonrpc_server_parent_class)->finalize (object);
}

static void
jsonrpc_server_class_init (JsonrpcServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = jsonrpc_server_finalize;

  signals [HANDLE_CALL] =
    g_signal_new ("handle-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, handle_call),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  4,
                  JSONRPC_TYPE_CLIENT,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT,
                  G_TYPE_VARIANT);

  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonrpcServerClass, notification),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  JSONRPC_TYPE_CLIENT,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT);
}

static void
jsonrpc_server_init (JsonrpcServer *self)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);

  priv->clients = g_hash_table_new_full (NULL, NULL, g_object_unref, NULL);
}

JsonrpcServer *
jsonrpc_server_new (void)
{
  return g_object_new (JSONRPC_TYPE_SERVER, NULL);
}

static gboolean
jsonrpc_server_client_handle_call (JsonrpcServer *self,
                                   const gchar   *method,
                                   JsonNode      *id,
                                   JsonNode      *params,
                                   JsonrpcClient *client)
{
  gboolean ret;

  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (method != NULL);
  g_assert (id != NULL);
  g_assert (params != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  g_signal_emit (self, signals [HANDLE_CALL], 0, client, method, id, params, &ret);

  return ret;
}

static void
jsonrpc_server_client_notification (JsonrpcServer *self,
                                    const gchar   *method,
                                    JsonNode      *params,
                                    JsonrpcClient *client)
{
  g_assert (JSONRPC_IS_SERVER (self));
  g_assert (method != NULL);
  g_assert (params != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  g_signal_emit (self, signals [NOTIFICATION], 0, client, method, params);
}

void
jsonrpc_server_accept_io_stream (JsonrpcServer *self,
                                 GIOStream     *io_stream)
{
  JsonrpcServerPrivate *priv = jsonrpc_server_get_instance_private (self);
  JsonrpcClient *client;

  g_return_if_fail (JSONRPC_IS_SERVER (self));
  g_return_if_fail (G_IS_IO_STREAM (io_stream));

  client = jsonrpc_client_new (io_stream);

  g_signal_connect_object (client,
                           "handle-call",
                           G_CALLBACK (jsonrpc_server_client_handle_call),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "notification",
                           G_CALLBACK (jsonrpc_server_client_notification),
                           self,
                           G_CONNECT_SWAPPED);

  g_hash_table_insert (priv->clients, client, NULL);

  jsonrpc_client_start_listening (client);
}
