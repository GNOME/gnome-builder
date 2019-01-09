/* gb-color-picker-prefs-palette-row.h
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include <dazzle.h>

G_BEGIN_DECLS

#define GB_TYPE_COLOR_PICKER_PREFS_PALETTE_ROW (gb_color_picker_prefs_palette_row_get_type())

G_DECLARE_FINAL_TYPE (GbColorPickerPrefsPaletteRow, gb_color_picker_prefs_palette_row, GB, COLOR_PICKER_PREFS_PALETTE_ROW, DzlPreferencesBin)

gboolean                      gb_color_picker_prefs_palette_row_get_needs_attention (GbColorPickerPrefsPaletteRow    *self);
void                          gb_color_picker_prefs_palette_row_set_needs_attention (GbColorPickerPrefsPaletteRow    *self,
                                                                                     gboolean                         needs_attention);
GbColorPickerPrefsPaletteRow *gb_color_picker_prefs_palette_row_new                 (void);

G_END_DECLS
