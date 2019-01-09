/* gstyle-color-panel.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include "gstyle-color-filter.h"
#include "gstyle-palette.h"
#include "gstyle-palette-widget.h"
#include "gstyle-xyz.h"


#define GSTYLE_TYPE_COLOR_PANEL_PREFS                 (gstyle_color_panel_prefs_get_type())
#define GSTYLE_TYPE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS (gstyle_color_panel_strings_visible_flags_get_type ())
#define GSTYLE_TYPE_COLOR_PANEL                       (gstyle_color_panel_get_type())

typedef enum
{
  GSTYLE_COLOR_PANEL_PREFS_COMPONENTS,
  GSTYLE_COLOR_PANEL_PREFS_COLOR_STRINGS,
  GSTYLE_COLOR_PANEL_PREFS_PALETTES,
  GSTYLE_COLOR_PANEL_PREFS_PALETTES_LIST
} GstyleColorPanelPrefs;

typedef enum
{
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_NONE = 0,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX3 = 1 << 0,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX6 = 1 << 1,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGB  = 1 << 2,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGBA = 1 << 3,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSL  = 1 << 4,
  GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSLA = 1 << 5
} GstyleColorPanelStringsVisibleFlags;

G_DECLARE_FINAL_TYPE (GstyleColorPanel, gstyle_color_panel, GSTYLE, COLOR_PANEL, GtkBox)

GType                       gstyle_color_panel_prefs_get_type                    (void);
GType                       gstyle_color_panel_strings_visible_flags_get_type    (void);

GstyleColorPanel           *gstyle_color_panel_new                         (void);
GstyleColorFilter           gstyle_color_panel_get_filter                  (GstyleColorPanel    *self);
GstylePaletteWidget        *gstyle_color_panel_get_palette_widget          (GstyleColorPanel    *self);
void                        gstyle_color_panel_get_rgba                    (GstyleColorPanel    *self,
                                                                            GdkRGBA             *rgba);
void                        gstyle_color_panel_set_filter                  (GstyleColorPanel    *self,
                                                                            GstyleColorFilter    filter);
void                        gstyle_color_panel_set_rgba                    (GstyleColorPanel    *self,
                                                                            const GdkRGBA       *rgba);
void                        gstyle_color_panel_get_xyz                     (GstyleColorPanel    *self,
                                                                            GstyleXYZ           *xyz);
void                        gstyle_color_panel_set_xyz                     (GstyleColorPanel    *self,
                                                                            const GstyleXYZ     *xyz);
void                        gstyle_color_panel_set_prefs_pages             (GstyleColorPanel    *self,
                                                                            GtkWidget           *components_page,
                                                                            GtkWidget           *colorstrings_page,
                                                                            GtkWidget           *palettes_page,
                                                                            GtkWidget           *palettes_list_page);
void                        gstyle_color_panel_show_palette                (GstyleColorPanel    *self,
                                                                            GstylePalette       *palette);

G_END_DECLS
