/* gstyle-color.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gdk/gdk.h>

#include "gstyle-types.h"

G_BEGIN_DECLS

#define GSTYLE_TYPE_COLOR (gstyle_color_get_type())
#define GSTYLE_TYPE_COLOR_KIND (gstyle_color_kind_get_type())
#define GSTYLE_TYPE_COLOR_UNIT (gstyle_color_unit_get_type())

G_DECLARE_FINAL_TYPE (GstyleColor, gstyle_color, GSTYLE, COLOR, GObject)

typedef enum
{
  GSTYLE_COLOR_KIND_UNKNOW,
  GSTYLE_COLOR_KIND_ORIGINAL,
  GSTYLE_COLOR_KIND_RGB_HEX6,
  GSTYLE_COLOR_KIND_RGB_HEX3,
  GSTYLE_COLOR_KIND_RGB,
  GSTYLE_COLOR_KIND_RGB_PERCENT,
  GSTYLE_COLOR_KIND_RGBA,
  GSTYLE_COLOR_KIND_RGBA_PERCENT,
  GSTYLE_COLOR_KIND_HSL,
  GSTYLE_COLOR_KIND_HSLA,
  GSTYLE_COLOR_KIND_PREDEFINED
} GstyleColorKind;

typedef enum
{
  GSTYLE_COLOR_UNIT_NONE,
  GSTYLE_COLOR_UNIT_PERCENT,
  GSTYLE_COLOR_UNIT_VALUE
} GstyleColorUnit;

GType                gstyle_color_kind_get_type               (void);
GType                gstyle_color_unit_get_type               (void);

GstyleColor         *gstyle_color_copy                        (GstyleColor      *self);
void                 gstyle_color_fill                        (GstyleColor      *src_color,
                                                               GstyleColor      *dst_color);
GstyleColor         *gstyle_color_new                         (const gchar      *name,
                                                               GstyleColorKind   kind,
                                                               guint             red,
                                                               guint             green,
                                                               guint             blue,
                                                               guint             alpha);
GstyleColor         *gstyle_color_new_from_rgba               (const gchar      *name,
                                                               GstyleColorKind   kind,
                                                               GdkRGBA          *rgba);
GstyleColor         *gstyle_color_new_from_hsla               (const gchar      *name,
                                                               GstyleColorKind   kind,
                                                               gdouble           hue,
                                                               gdouble           saturation,
                                                               gdouble           lightness,
                                                               gdouble           alpha);
GstyleColor         *gstyle_color_new_from_string             (const gchar      *name,
                                                               const gchar      *color_string);

/* TODO: add an async functions */
GPtrArray           *gstyle_color_fuzzy_parse_color_string    (const gchar      *color_string);
GPtrArray           *gstyle_color_parse                       (const gchar      *string);
gboolean             gstyle_color_parse_color_string          (const gchar      *color_string,
                                                               GdkRGBA          *rgba,
                                                               GstyleColorKind  *kind);
void                 gstyle_color_to_hsla                     (GstyleColor      *self,
                                                               gdouble          *hue,
                                                               gdouble          *saturation,
                                                               gdouble          *lightness,
                                                               gdouble          *alpha);
gchar               *gstyle_color_to_string                   (GstyleColor      *self,
                                                               GstyleColorKind   kind);
GstyleColorKind      gstyle_color_get_kind                    (GstyleColor      *self);
const gchar         *gstyle_color_get_name                    (GstyleColor      *self);
GdkRGBA             *gstyle_color_get_rgba                    (GstyleColor      *self);
void                 gstyle_color_set_kind                    (GstyleColor      *self,
                                                               GstyleColorKind   kind);
void                 gstyle_color_set_name                    (GstyleColor      *self,
                                                               const gchar      *name);
void                 gstyle_color_set_rgba                    (GstyleColor      *self,
                                                               GdkRGBA          *rgba);
void                 gstyle_color_set_alpha                   (GstyleColor      *self,
                                                               gdouble           alpha);
void                 gstyle_color_fill_rgba                   (GstyleColor      *self,
                                                               GdkRGBA          *rgba);

G_END_DECLS
