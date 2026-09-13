/* gbp-acp-terminal.h
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

#define GBP_TYPE_ACP_TERMINAL (gbp_acp_terminal_get_type())

G_DECLARE_FINAL_TYPE (GbpAcpTerminal, gbp_acp_terminal, GBP, ACP_TERMINAL, GObject)

GbpAcpTerminal *gbp_acp_terminal_new          (const gchar        *command,
                                               const gchar *const *args,
                                               guint               n_args,
                                               const gchar *const *env_names,
                                               const gchar *const *env_values,
                                               guint               n_env,
                                               const gchar        *cwd,
                                               guint64             output_byte_limit);
const gchar    *gbp_acp_terminal_get_id       (GbpAcpTerminal     *self);
void            gbp_acp_terminal_start        (GbpAcpTerminal     *self);
gboolean        gbp_acp_terminal_get_exited   (GbpAcpTerminal     *self);
gint            gbp_acp_terminal_get_exit_code (GbpAcpTerminal    *self);
const gchar    *gbp_acp_terminal_get_signal   (GbpAcpTerminal     *self);
const gchar    *gbp_acp_terminal_get_output   (GbpAcpTerminal     *self);
gboolean        gbp_acp_terminal_get_truncated (GbpAcpTerminal    *self);
void            gbp_acp_terminal_wait_async   (GbpAcpTerminal     *self,
                                               GCancellable       *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer            user_data);
gboolean        gbp_acp_terminal_wait_finish  (GbpAcpTerminal     *self,
                                               GAsyncResult       *result,
                                               GError            **error);
void            gbp_acp_terminal_kill         (GbpAcpTerminal     *self);

G_END_DECLS
