/* jsonrpc-client.h
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

#ifndef JSONRPC_CLIENT_H
#define JSONRPC_CLIENT_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define JSONRPC_TYPE_CLIENT  (jsonrpc_client_get_type())
#define JSONRPC_CLIENT_ERROR (jsonrpc_client_error_quark())

G_DECLARE_DERIVABLE_TYPE (JsonrpcClient, jsonrpc_client, JSONRPC, CLIENT, GObject)

struct _JsonrpcClientClass
{
  GObjectClass parent_class;

  void     (*notification) (JsonrpcClient *self,
                            const gchar   *method_name,
                            GVariant      *params);
  gboolean (*handle_call)  (JsonrpcClient *self,
                            const gchar   *method,
                            GVariant      *id,
                            GVariant      *params);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

GQuark         jsonrpc_client_error_quark              (void);
JsonrpcClient *jsonrpc_client_new                      (GIOStream            *io_stream);
gboolean       jsonrpc_client_get_use_gvariant         (JsonrpcClient        *self);
void           jsonrpc_client_set_use_gvariant         (JsonrpcClient        *self,
                                                        gboolean              use_gvariant);
gboolean       jsonrpc_client_close                    (JsonrpcClient        *self,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void           jsonrpc_client_close_async              (JsonrpcClient        *self,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean       jsonrpc_client_close_finish             (JsonrpcClient        *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
gboolean       jsonrpc_client_call                     (JsonrpcClient        *self,
                                                        const gchar          *method,
                                                        GVariant             *params,
                                                        GCancellable         *cancellable,
                                                        GVariant            **return_value,
                                                        GError              **error);
void           jsonrpc_client_call_async               (JsonrpcClient        *self,
                                                        const gchar          *method,
                                                        GVariant             *params,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean       jsonrpc_client_call_finish              (JsonrpcClient        *self,
                                                        GAsyncResult         *result,
                                                        GVariant            **return_value,
                                                        GError              **error);
gboolean       jsonrpc_client_send_notification        (JsonrpcClient        *self,
                                                        const gchar          *method,
                                                        GVariant             *params,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void           jsonrpc_client_send_notification_async  (JsonrpcClient        *self,
                                                        const gchar          *method,
                                                        GVariant             *params,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean       jsonrpc_client_send_notification_finish (JsonrpcClient        *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
gboolean       jsonrpc_client_reply                    (JsonrpcClient        *self,
                                                        GVariant             *id,
                                                        GVariant             *result,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
void           jsonrpc_client_reply_async              (JsonrpcClient        *self,
                                                        GVariant             *id,
                                                        GVariant             *result,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
gboolean       jsonrpc_client_reply_finish             (JsonrpcClient        *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);
void           jsonrpc_client_start_listening          (JsonrpcClient        *self);

G_END_DECLS

#endif /* JSONRPC_CLIENT_H */
