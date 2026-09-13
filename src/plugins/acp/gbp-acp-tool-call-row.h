/* gbp-acp-tool-call-row.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GBP_TYPE_ACP_TOOL_CALL_ROW (gbp_acp_tool_call_row_get_type())

G_DECLARE_FINAL_TYPE (GbpAcpToolCallRow, gbp_acp_tool_call_row, GBP, ACP_TOOL_CALL_ROW, GtkBox)

GtkWidget   *gbp_acp_tool_call_row_new     (const gchar       *tool_call_id,
                                            const gchar       *title,
                                            const gchar       *kind);
const gchar *gbp_acp_tool_call_row_get_id  (GbpAcpToolCallRow *self);
void         gbp_acp_tool_call_row_update  (GbpAcpToolCallRow *self,
                                            GVariant          *update);

G_END_DECLS
