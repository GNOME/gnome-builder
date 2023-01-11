/* gbp-shortcutui-row.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <adwaita.h>

G_BEGIN_DECLS

#define GBP_TYPE_SHORTCUTUI_ROW (gbp_shortcutui_row_get_type())

G_DECLARE_FINAL_TYPE (GbpShortcutuiRow, gbp_shortcutui_row, GBP, SHORTCUTUI_ROW, AdwActionRow)

const char *gbp_shortcutui_row_get_accelerator (GbpShortcutuiRow *self);
void        gbp_shortcutui_row_update_header   (GbpShortcutuiRow *self,
                                                GbpShortcutuiRow *before);

G_END_DECLS
