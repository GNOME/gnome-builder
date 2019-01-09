/* gstyle-color-plane.h
 *
 * based on : gtk-color-plane
 *   GTK - The GIMP Toolkit
 *   Copyright 2012 Red Hat, Inc.
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

#include "gstyle-color.h"
#include "gstyle-color-component.h"
#include "gstyle-color-convert.h"
#include "gstyle-color-filter.h"
#include "gstyle-xyz.h"

G_BEGIN_DECLS

#define GSTYLE_TYPE_COLOR_PLANE (gstyle_color_plane_get_type())
#define GSTYLE_TYPE_COLOR_PLANE_MODE (gstyle_color_plane_mode_get_type())

G_DECLARE_DERIVABLE_TYPE (GstyleColorPlane, gstyle_color_plane, GSTYLE, COLOR_PLANE, GtkDrawingArea)

typedef enum
{
  GSTYLE_COLOR_PLANE_MODE_HUE,
  GSTYLE_COLOR_PLANE_MODE_SATURATION,
  GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS,
  GSTYLE_COLOR_PLANE_MODE_CIELAB_L,
  GSTYLE_COLOR_PLANE_MODE_CIELAB_A,
  GSTYLE_COLOR_PLANE_MODE_CIELAB_B,
  GSTYLE_COLOR_PLANE_MODE_RED,
  GSTYLE_COLOR_PLANE_MODE_GREEN,
  GSTYLE_COLOR_PLANE_MODE_BLUE,
  GSTYLE_COLOR_PLANE_MODE_NONE
} GstyleColorPlaneMode;

struct _GstyleColorPlaneClass
{
  GtkDrawingAreaClass parent;
};

GType                  gstyle_color_plane_mode_get_type                 (void);

GstyleColorPlane      *gstyle_color_plane_new                           (void);
GtkAdjustment         *gstyle_color_plane_get_component_adjustment      (GstyleColorPlane       *self,
                                                                         GstyleColorComponent    comp);
GstyleColorFilterFunc  gstyle_color_plane_get_filter_func               (GstyleColorPlane       *self);
void                   gstyle_color_plane_get_filtered_rgba             (GstyleColorPlane       *self,
                                                                         GdkRGBA                *rgba);
void                   gstyle_color_plane_get_rgba                      (GstyleColorPlane       *self,
                                                                         GdkRGBA                *rgba);
void                   gstyle_color_plane_get_xyz                       (GstyleColorPlane       *self,
                                                                         GstyleXYZ              *xyz);
void                   gstyle_color_plane_set_filter_func               (GstyleColorPlane       *self,
                                                                         GstyleColorFilterFunc   filter_cb,
                                                                         gpointer                user_data);
void                   gstyle_color_plane_set_mode                      (GstyleColorPlane       *self,
                                                                         GstyleColorPlaneMode    mode);
void                   gstyle_color_plane_set_preferred_unit            (GstyleColorPlane       *self,
                                                                         GstyleColorUnit         preferred_unit);
void                   gstyle_color_plane_set_rgba                      (GstyleColorPlane       *self,
                                                                         const GdkRGBA          *rgba);
void                   gstyle_color_plane_set_xyz                       (GstyleColorPlane       *self,
                                                                         const GstyleXYZ        *xyz);

G_END_DECLS
