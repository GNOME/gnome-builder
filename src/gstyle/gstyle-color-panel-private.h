/* gstyle-color-panel-private.h
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

#include <dazzle.h>

#include "gstyle-cielab.h"
#include "gstyle-color.h"
#include "gstyle-color-convert.h"
#include "gstyle-color-component.h"
#include "gstyle-color-plane.h"
#include "gstyle-color-scale.h"
#include "gstyle-color-widget.h"
#include "gstyle-css-provider.h"
#include "gstyle-eyedropper.h"
#include "gstyle-color-filter.h"
#include "gstyle-palette-widget.h"
#include "gstyle-revealer.h"
#include "gstyle-slidein.h"
#include "gstyle-utils.h"

#include "gstyle-color-panel.h"

G_BEGIN_DECLS

#define PREFS_COMPONENTS_PAGE    "components-page"
#define PREFS_COLOR_STRINGS_PAGE "colorstrings-page"
#define PREFS_PALETTES_PAGE      "palettes-page"
#define PREFS_PALETTES_LIST_PAGE "paletteslist-page"

typedef struct _ColorComp
{
  GtkToggleButton  *toggle;
  GtkSpinButton    *spin;
  GstyleColorScale *scale;

  gulong            toggle_handler_id;
} ColorComp;

struct _GstyleColorPanel
{
  GtkBox                               parent_instance;

  GstyleCssProvider                   *default_provider;

  GstyleColorPlane                    *color_plane;
  GtkAdjustment                       *adj_alpha;

  GstyleColor                         *new_color;
  GstyleColor                         *old_color;
  GstyleColorWidget                   *new_swatch;
  GstyleColorWidget                   *old_swatch;

  GtkButton                           *picker_button;
  GstyleEyedropper                    *eyedropper;
  GtkWidget                           *search_color_entry;
  GtkWidget                           *search_strings_popover;
  GtkWidget                           *search_strings_list;

  DzlFuzzyMutableIndex                *fuzzy;

  GtkToggleButton                     *components_toggle;
  GtkToggleButton                     *strings_toggle;
  GtkToggleButton                     *palette_toggle;

  GtkWidget                           *hsv_grid;
  GtkWidget                           *lab_grid;
  GtkWidget                           *rgb_grid;

  GstyleRevealer                      *scale_reveal;
  GstyleRevealer                      *string_reveal;
  GstyleRevealer                      *palette_reveal;

  GtkWidget                           *components_controls;
  GtkWidget                           *strings_controls;
  GtkWidget                           *palette_controls;

  GstyleColorScale                    *ref_scale;
  GstyleColorScale                    *alpha_scale;

  GtkLabel                            *res_hex3_label;
  GtkLabel                            *res_hex6_label;
  GtkLabel                            *res_rgb_label;
  GtkLabel                            *res_rgba_label;
  GtkLabel                            *res_hsl_label;
  GtkLabel                            *res_hsla_label;

  GtkLabel                            *hex3_label;
  GtkLabel                            *hex6_label;
  GtkLabel                            *rgb_label;
  GtkLabel                            *rgba_label;
  GtkLabel                            *hsl_label;
  GtkLabel                            *hsla_label;

  GstylePaletteWidget                 *palette_widget;

  GIcon                               *degree_icon;
  GIcon                               *percent_icon;

  GtkToggleButton                     *components_prefs_button;
  GtkToggleButton                     *color_strings_prefs_button;
  GtkToggleButton                     *palettes_prefs_button;
  GtkToggleButton                     *palettes_list_prefs_button;
  GtkToggleButton                     *last_checked_prefs_button;

  gulong                               components_prefs_button_handler_id;
  gulong                               color_strings_prefs_button_handler_id;
  gulong                               palettes_prefs_button_handler_id;
  gulong                               palettes_list_prefs_button_handler_id;

  GtkWidget                           *components_prefs_bin;
  GtkWidget                           *color_strings_prefs_bin;
  GtkWidget                           *palettes_prefs_bin;
  GtkWidget                           *palettes_list_prefs_bin;

  ColorComp                            components [N_GSTYLE_COLOR_COMPONENT];
  GstyleColorComponent                 current_comp;
  GstyleColorUnit                      preferred_unit;
  GstyleColorFilter                    filter;

  GstyleSlidein                       *prefs_slidein;
  GtkStack                            *prefs_stack;
  GtkWidget                           *last_toggled_prefs;

  GstyleColorPanelStringsVisibleFlags  strings_visible_flags;
};

void                  _gstyle_color_panel_update_prefs_page                   (GstyleColorPanel       *self,
                                                                               const gchar            *page_name);

G_END_DECLS
