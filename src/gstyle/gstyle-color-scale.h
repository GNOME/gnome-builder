/* gstyle-color-scale.h
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
#include <gdk/gdk.h>

#include "gstyle-color-filter.h"

G_BEGIN_DECLS

#define GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE (256)
#define GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE (GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE * 4)

typedef enum
{
  GSTYLE_COLOR_SCALE_KIND_HUE,
  GSTYLE_COLOR_SCALE_KIND_GREY,
  GSTYLE_COLOR_SCALE_KIND_ALPHA,
  GSTYLE_COLOR_SCALE_KIND_RED,
  GSTYLE_COLOR_SCALE_KIND_GREEN,
  GSTYLE_COLOR_SCALE_KIND_BLUE,
  GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS,
  GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA
} GstyleColorScaleKind;

#define GSTYLE_TYPE_COLOR_SCALE_KIND (gstyle_color_scale_kind_get_type())
#define GSTYLE_TYPE_COLOR_SCALE      (gstyle_color_scale_get_type())

G_DECLARE_FINAL_TYPE (GstyleColorScale, gstyle_color_scale, GSTYLE, COLOR_SCALE, GtkScale)

GType                   gstyle_color_scale_kind_get_type               (void);

GstyleColorScale       *gstyle_color_scale_new                         (GtkAdjustment         *adjustment);
gint                    gstyle_color_scale_add_rgba_color_stop         (GstyleColorScale      *self,
                                                                        gdouble                offset,
                                                                        GdkRGBA               *rgba);
gint                    gstyle_color_scale_add_color_stop              (GstyleColorScale      *self,
                                                                        gdouble                offset,
                                                                        gdouble                red,
                                                                        gdouble                green,
                                                                        gdouble                blue,
                                                                        gdouble                alpha);
void                    gstyle_color_scale_clear_color_stops           (GstyleColorScale      *self);
GstyleColorFilterFunc   gstyle_color_scale_get_filter_func             (GstyleColorScale      *self);
GstyleColorScaleKind    gstyle_color_scale_get_kind                    (GstyleColorScale      *self);

gboolean                gstyle_color_scale_remove_color_stop           (GstyleColorScale      *self,
                                                                        gint                   id);
void                    gstyle_color_scale_set_custom_data             (GstyleColorScale      *self,
                                                                        guint32               *data);
void                    gstyle_color_scale_set_filter_func             (GstyleColorScale      *self,
                                                                        GstyleColorFilterFunc  filter_cb,
                                                                        gpointer               user_data);
void                    gstyle_color_scale_set_kind                    (GstyleColorScale      *self,
                                                                        GstyleColorScaleKind   kind);

G_END_DECLS
