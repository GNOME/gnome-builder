/* gstyle-color-filter.c
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

#include "gstyle-color-filter.h"

static gdouble web_colors [] = {0.0, 0.2, 0.2, 0.4, 0.4 , 0.6 , 0.6, 0.8, 0.8, 1.0, 1.0};
#define TO_WEB_COLOR(x) (web_colors [(gint)(x * 10.0)])

/**
 * gstyle_color_filter_func:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A WebSafe Color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_websafe (GdkRGBA  *rgba,
                             GdkRGBA  *filter_rgba,
                             gpointer  user_data)
{
  filter_rgba->red = TO_WEB_COLOR (rgba->red);
  filter_rgba->green = TO_WEB_COLOR (rgba->green);
  filter_rgba->blue  = TO_WEB_COLOR (rgba->blue);
  filter_rgba->alpha = rgba->alpha;
}
