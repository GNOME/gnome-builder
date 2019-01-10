/* gbp-meson-tool-row.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_MESON_TOOL_ROW (gbp_meson_tool_row_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonToolRow, gbp_meson_tool_row, GBP, MESON_TOOL_ROW, GtkListBoxRow)

GbpMesonToolRow  *gbp_meson_tool_row_new (const gchar *tool_id,
                                          const gchar *tool_path,
                                          const gchar *lang_id);

const gchar      *gbp_meson_tool_row_get_tool_id (GbpMesonToolRow *tool_row);

G_END_DECLS
