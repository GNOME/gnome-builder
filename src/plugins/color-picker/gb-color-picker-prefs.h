/* gb-color-picker-prefs.h
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

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gstyle-color-panel.h"

G_BEGIN_DECLS

#define GB_TYPE_COLOR_PICKER_PREFS (gb_color_picker_prefs_get_type())

G_DECLARE_FINAL_TYPE (GbColorPickerPrefs, gb_color_picker_prefs, GB, COLOR_PICKER_PREFS, GObject)

GbColorPickerPrefs    *gb_color_picker_prefs_new                (void);

GtkWidget             *gb_color_picker_prefs_get_page           (GbColorPickerPrefs     *self,
                                                                 GstyleColorPanelPrefs   prefs_type);

GstyleColorPanel      *gb_color_picker_prefs_get_panel          (GbColorPickerPrefs     *self);
void                   gb_color_picker_prefs_set_panel          (GbColorPickerPrefs     *self,
                                                                 GstyleColorPanel       *panel);

G_END_DECLS
