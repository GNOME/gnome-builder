/* gstyle-color-widget.h
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

#include "gstyle-color.h"
#include "gstyle-color-filter.h"

G_BEGIN_DECLS

typedef enum
{
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NONE = 0,
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND = 1 << 0,
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME = 1 << 1,
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA = 1 << 2,
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_COLOR = 1 << 3,
  GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALL  =
    (GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND |
     GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME |
     GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA |
     GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_COLOR)
} GstyleColorWidgetDndLockFlags;

#define GSTYLE_TYPE_COLOR_WIDGET (gstyle_color_widget_get_type())
#define GSTYLE_TYPE_COLOR_WIDGET_DND_LOCK_FLAGS (gstyle_color_widget_dnd_lock_flags_get_type ())

G_DECLARE_FINAL_TYPE (GstyleColorWidget, gstyle_color_widget, GSTYLE, COLOR_WIDGET, GtkBin)

GType                  gstyle_color_widget_dnd_lock_flags_get_type   (void);

GstyleColorWidget     *gstyle_color_widget_new                       (void);
GstyleColorWidget     *gstyle_color_widget_copy                      (GstyleColorWidget      *self);
GstyleColorWidget     *gstyle_color_widget_new_with_color            (GstyleColor            *color);

gboolean               gstyle_color_widget_get_name_visible          (GstyleColorWidget      *self);
gboolean               gstyle_color_widget_get_fallback_name_visible (GstyleColorWidget      *self);
GstyleColorKind        gstyle_color_widget_get_fallback_name_kind    (GstyleColorWidget      *self);
GstyleColor           *gstyle_color_widget_get_color                 (GstyleColorWidget      *self);
GstyleColor           *gstyle_color_widget_get_filtered_color        (GstyleColorWidget      *self);
GstyleColorFilterFunc  gstyle_color_widget_get_filter_func           (GstyleColorWidget      *self);

void                   gstyle_color_widget_set_fallback_name_visible (GstyleColorWidget      *self,
                                                                      gboolean                visible);
void                   gstyle_color_widget_set_fallback_name_kind    (GstyleColorWidget      *self,
                                                                      GstyleColorKind         kind);
void                   gstyle_color_widget_set_name_visible          (GstyleColorWidget      *self,
                                                                      gboolean                visible);
void                   gstyle_color_widget_set_color                 (GstyleColorWidget      *self,
                                                                      GstyleColor            *color);
void                   gstyle_color_widget_set_filter_func           (GstyleColorWidget      *self,
                                                                      GstyleColorFilterFunc   filter_func,
                                                                      gpointer                user_data);

G_END_DECLS
