/* gstyle-color-filter.h
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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
 */

#ifndef GSTYLE_COLOR_FILTER_H
#define GSTYLE_COLOR_FILTER_H

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef void (*GstyleColorFilter)(GdkRGBA *rgba, GdkRGBA *filter_rgba, gpointer user_data);

void           gstyle_color_filter_websafe        (GdkRGBA          *rgba,
                                                   GdkRGBA          *filter_rgba,
                                                   gpointer          user_data);

G_END_DECLS

#endif /* GSTYLE_COLOR_FILTER_H */

