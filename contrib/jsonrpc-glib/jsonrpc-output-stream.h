/* jsonrpc-output-stream.h
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

#ifndef JSONRPC_OUTPUT_STREAM_H
#define JSONRPC_OUTPUT_STREAM_H

#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define JSONRPC_TYPE_OUTPUT_STREAM (jsonrpc_output_stream_get_type())

G_DECLARE_DERIVABLE_TYPE (JsonrpcOutputStream, jsonrpc_output_stream, JSONRPC, OUTPUT_STREAM, GDataOutputStream)

struct _JsonrpcOutputStreamClass
{
  GDataOutputStreamClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

JsonrpcOutputStream *jsonrpc_output_stream_new                  (GOutputStream        *base_stream);
gboolean             jsonrpc_output_stream_write_message        (JsonrpcOutputStream  *self,
                                                                 JsonNode             *node,
                                                                 GCancellable         *cancellable,
                                                                 GError              **error);
void                 jsonrpc_output_stream_write_message_async  (JsonrpcOutputStream  *self,
                                                                 JsonNode             *node,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean             jsonrpc_output_stream_write_message_finish (JsonrpcOutputStream  *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);

G_END_DECLS

#endif /* JSONRPC_OUTPUT_STREAM_H */
