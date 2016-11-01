/* jsonrpc-server.h
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

#ifndef JSONRPC_SERVER_H
#define JSONRPC_SERVER_H

#include <json-glib/json-glib.h>
#include <gio/gio.h>

#include "jsonrpc-client.h"

G_BEGIN_DECLS

#define JSONRPC_TYPE_SERVER (jsonrpc_server_get_type())

G_DECLARE_DERIVABLE_TYPE (JsonrpcServer, jsonrpc_server, JSONRPC, SERVER, GObject)

struct _JsonrpcServerClass
{
  GObjectClass parent_class;

  gboolean (*handle_call)  (JsonrpcServer *self,
                            JsonrpcClient *client,
                            const gchar   *method,
                            JsonNode      *id,
                            JsonNode      *params);
  void     (*notification) (JsonrpcServer *self,
                            JsonrpcClient *client,
                            const gchar   *method,
                            JsonNode      *params);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

JsonrpcServer *jsonrpc_server_new              (void);
void           jsonrpc_server_accept_io_stream (JsonrpcServer *self,
                                                GIOStream     *stream);

G_END_DECLS

#endif /* JSONRPC_SERVER_H */
