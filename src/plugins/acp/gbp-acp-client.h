/* gbp-acp-client.h
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

#include <libide-core.h>

G_BEGIN_DECLS

#define GBP_TYPE_ACP_CLIENT (gbp_acp_client_get_type())

G_DECLARE_FINAL_TYPE (GbpAcpClient, gbp_acp_client, GBP, ACP_CLIENT, GObject)

GbpAcpClient *gbp_acp_client_new                        (IdeContext   *context);
void          gbp_acp_client_start                      (GbpAcpClient *self);
void          gbp_acp_client_stop                       (GbpAcpClient *self);
gboolean      gbp_acp_client_is_ready                   (GbpAcpClient *self);
gboolean      gbp_acp_client_is_busy                    (GbpAcpClient *self);
const gchar  *gbp_acp_client_get_session_id             (GbpAcpClient *self);
const gchar  *gbp_acp_client_get_agent_name             (GbpAcpClient *self);
GVariant     *gbp_acp_client_dup_config_options         (GbpAcpClient *self);
void          gbp_acp_client_prompt_async               (GbpAcpClient *self,
                                                         const gchar  *text,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer      user_data);
gchar        *gbp_acp_client_prompt_finish              (GbpAcpClient *self,
                                                         GAsyncResult *result,
                                                         GError      **error);
void          gbp_acp_client_cancel                     (GbpAcpClient *self);
void          gbp_acp_client_reply_permission           (GbpAcpClient *self,
                                                         const gchar  *option_id);
void          gbp_acp_client_set_config_option          (GbpAcpClient *self,
                                                         const gchar  *config_id,
                                                         const gchar  *value);

G_END_DECLS
