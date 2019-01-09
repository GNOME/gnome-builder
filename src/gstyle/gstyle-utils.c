/* gstyle-utils.c
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

#include "gstyle-utils.h"

gboolean
gstyle_str_empty0 (const gchar *str)
{
  return (str == NULL) || (str[0] == '\0');
}

gboolean
gstyle_utf8_is_spaces (const gchar *str)
{
  gunichar c;

  if (str == NULL)
    return FALSE;

  while (g_unichar_isspace (c = g_utf8_get_char (str)))
    str = g_utf8_next_char (str);

  return (c == '\0');
}

typedef enum
{
  CORNER_TOP_LEFT,
  CORNER_TOP_RIGHT,
  CORNER_BOTTOM_LEFT,
  CORNER_BOTTOM_RIGHT
} Corner;

static void
draw_corner (cairo_t *cr,
             gdouble  x,
             gdouble  y,
             gdouble  radius,
             Corner   corner)
{
  switch (corner)
    {
    case CORNER_TOP_LEFT:
      cairo_arc (cr, x + radius, y + radius, radius, -G_PI, -G_PI_2);
      break;

    case CORNER_TOP_RIGHT:
      cairo_arc (cr, x - radius, y + radius, radius, -G_PI_2, 0.0);
      break;

    case CORNER_BOTTOM_RIGHT:
      cairo_arc (cr, x - radius, y - radius, radius, 0.0, G_PI_2);
      break;

    case CORNER_BOTTOM_LEFT:
      cairo_arc (cr, x + radius, y - radius, radius, G_PI_2, G_PI);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

void
draw_cairo_round_box (cairo_t      *cr,
                      GdkRectangle  rect,
                      gint          tl_radius,
                      gint          tr_radius,
                      gint          bl_radius,
                      gint          br_radius)
{
  gdouble right = rect.x + rect.width;
  gdouble bottom = rect.y + rect.height;

  cairo_new_sub_path (cr);
  cairo_move_to (cr, rect.x, rect.y + tl_radius);

  if (tl_radius > 0)
    draw_corner (cr, rect.x, rect.y, tl_radius, CORNER_TOP_LEFT);

  cairo_line_to (cr, right - tr_radius, rect.y);

  if (tr_radius > 0)
    draw_corner (cr, right, rect.y, tr_radius, CORNER_TOP_RIGHT);

  cairo_line_to (cr, right, bottom - br_radius);

  if (br_radius > 0)
    draw_corner (cr, right, bottom, br_radius, CORNER_BOTTOM_RIGHT);

  cairo_line_to (cr, rect.x + bl_radius, bottom);

  if (br_radius > 0)
    draw_corner (cr, rect.x, bottom, bl_radius, CORNER_BOTTOM_LEFT);

  cairo_close_path (cr);
}

void
gstyle_utils_get_rect_resized_box (GdkRectangle  src_rect,
                                   GdkRectangle *dst_rect,
                                   GtkBorder    *offset)
{
  dst_rect->x = src_rect.x + offset->left;
  dst_rect->y = src_rect.y + offset->top;
  dst_rect->width = src_rect.width - (offset->left + offset->right);
  dst_rect->height = src_rect.height - (offset->top + offset->bottom);

  if (dst_rect->width < 1)
    {
      dst_rect->width = 1;
      dst_rect->x = src_rect.x + src_rect.width / 2;
    }

  if (dst_rect->height < 1)
    {
      dst_rect->height = 1;
      dst_rect->y = src_rect.y + src_rect.height / 2;
    }
}

cairo_pattern_t *
gstyle_utils_get_checkered_pattern (void)
{
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;

  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_A8, 2, 2, 4);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);
  cairo_surface_destroy (surface);

  return pattern;
}

void
gstyle_utils_get_contrasted_rgba (GdkRGBA  rgba,
                                  GdkRGBA *dst_rgba)
{
  guint brightness;

  brightness = rgba.red * 299 + rgba.green * 587 + rgba.blue * 114;
  if (brightness > 500)
    {
      dst_rgba->red = dst_rgba->green = dst_rgba->blue = 0.0;
    }
  else
    {
      dst_rgba->red = dst_rgba->green = dst_rgba->blue = 1.0;
    }

  rgba.alpha = 1.0;
}

gboolean
gstyle_utils_is_array_contains_same_color (GPtrArray   *ar,
                                           GstyleColor *color)
{
  GstyleColor *tmp_color;
  GdkRGBA color_rgba;
  GdkRGBA tmp_rgba;

  g_return_val_if_fail (GSTYLE_IS_COLOR (color), FALSE);
  g_return_val_if_fail (ar != NULL, FALSE);

  gstyle_color_fill_rgba (color, &color_rgba);

  for (gint i = 0; i < ar->len; ++i)
    {
      tmp_color = g_ptr_array_index (ar, i);
      gstyle_color_fill_rgba (tmp_color, &tmp_rgba);
      if (gdk_rgba_equal (&color_rgba, &tmp_rgba))
        return TRUE;
    }

  return FALSE;
}
