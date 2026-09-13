/* gbp-acp-jsonrpc.h
 *
 * Copyright 2026 GNOME Builder contributors
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define GBP_TYPE_ACP_JSONRPC (gbp_acp_jsonrpc_get_type())

G_DECLARE_FINAL_TYPE (GbpAcpJsonrpc, gbp_acp_jsonrpc, GBP, ACP_JSONRPC, GObject)

GbpAcpJsonrpc *gbp_acp_jsonrpc_new                  (GInputStream         *input,
                                                     GOutputStream        *output);
void           gbp_acp_jsonrpc_start                (GbpAcpJsonrpc        *self);
void           gbp_acp_jsonrpc_close                (GbpAcpJsonrpc        *self);
void           gbp_acp_jsonrpc_call_async           (GbpAcpJsonrpc        *self,
                                                     const gchar          *method,
                                                     GVariant             *params,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
GVariant      *gbp_acp_jsonrpc_call_finish          (GbpAcpJsonrpc        *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);
void           gbp_acp_jsonrpc_send_notification    (GbpAcpJsonrpc        *self,
                                                     const gchar          *method,
                                                     GVariant             *params);
void           gbp_acp_jsonrpc_reply                (GbpAcpJsonrpc        *self,
                                                     GVariant             *id,
                                                     GVariant             *result);
void           gbp_acp_jsonrpc_reply_error          (GbpAcpJsonrpc        *self,
                                                     GVariant             *id,
                                                     gint                  code,
                                                     const gchar          *message);

G_END_DECLS
