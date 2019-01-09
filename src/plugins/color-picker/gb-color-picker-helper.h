/*`gb-color-picker-helper.h
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

G_BEGIN_DECLS

void                      gb_color_picker_helper_change_color_tag                 (GtkTextTag       *tag,
                                                                                   GstyleColor      *color);
GtkTextTag               *gb_color_picker_helper_create_color_tag                 (GtkTextBuffer    *buffer,
                                                                                   GstyleColor      *color);
GtkTextTag               *gb_color_picker_helper_get_tag_at_iter                  (GtkTextIter      *cursor,
                                                                                   GstyleColor     **current_color,
                                                                                   GtkTextIter      *begin,
                                                                                   GtkTextIter      *end);
const gchar              *gb_color_picker_helper_get_color_picker_data_path       (void);
void                      gb_color_picker_helper_get_matching_monochrome          (GdkRGBA          *src_rgba,
                                                                                   GdkRGBA          *dst_rgba);
GtkTextTag               *gb_color_picker_helper_set_color_tag                    (GtkTextIter      *begin,
                                                                                   GtkTextIter      *end,
                                                                                   GstyleColor      *color,
                                                                                   gboolean          preserve_cursor);
GtkTextTag               *gb_color_picker_helper_set_color_tag_at_iter            (GtkTextIter      *iter,
                                                                                   GstyleColor      *color,
                                                                                   gboolean          preserve_cursor);

G_END_DECLS
