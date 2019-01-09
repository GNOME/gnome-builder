/* gstyle-color-item.h
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

#include <glib.h>

#include "gstyle-types.h"
#include "gstyle-color.h"

G_BEGIN_DECLS

#define GSTYLE_TYPE_COLOR_ITEM (gstyle_color_item_get_type ())

struct _GstyleColorItem
{
  GstyleColor    *color;
  guint           start;
  guint           len;
  volatile gint   ref_count;
};

GType                gstyle_color_item_get_type                (void) G_GNUC_CONST;

GstyleColorItem     *gstyle_color_item_new                     (GstyleColor      *color,
                                                                gint              start,
                                                                gint              len);
GstyleColorItem     *gstyle_color_item_copy                    (GstyleColorItem  *self);
void                 gstyle_color_item_free                    (GstyleColorItem  *self);
GstyleColorItem     *gstyle_color_item_ref                     (GstyleColorItem  *self);
void                 gstyle_color_item_unref                   (GstyleColorItem  *self);

const GstyleColor   *gstyle_color_item_get_color               (GstyleColorItem  *self);
void                 gstyle_color_item_set_color               (GstyleColorItem  *self,
                                                                GstyleColor      *color);

guint                gstyle_color_item_get_start               (GstyleColorItem  *self);
guint                gstyle_color_item_get_len                 (GstyleColorItem  *self);

G_END_DECLS
