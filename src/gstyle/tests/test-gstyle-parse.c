/* test-gstyle-parse.c
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

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gstyle-color.h"
#include "gstyle-color-convert.h"
#include "gstyle-color-item.h"

typedef struct
{
  const gchar *rgb;

  gdouble h;
  gdouble s;
  gdouble l;
  gdouble a;
} ColorItem;

static ColorItem rgba_table[] =
{
  { "#000000",                    0,   0,   0, 1   },
  { "#102030",                   16,  32,  48, 1   },
  { "#FFFFFF",                  255, 255, 255, 1   },
  { "#808080",                  128, 128, 128, 1   },
  { "#1aF",                      17, 170, 255, 1   },
  { "rgb(100, 200, 50)",        100, 200,  50, 1   },
  { "rgb(10%, 50%, 70%)",        26, 128, 179, 1   },
  { "rgba(10%, 50%, 40%, 0.5)",  26, 128, 102, 0.5 },
  { "rgba(0, 10, 70, 1)",         0,  10,  70, 1   },
  { "hsl(100, 100%, 50%)",       85, 255,   0, 1   },
  { "hsl(250, 50%, 70%)",       153, 140, 217, 1   },
  { "hsla(40, 50%, 40%, 0.5)",  153, 119,  51, 0.5 },
  { "hsla(10, 10%, 70%, 1)",    186, 173, 171, 1   },
  { "aliceblue",                240, 248, 255, 1   },
  { "darkgray",                 169, 169, 169, 1   },
  { "peru",                     205, 133,  63, 1   },
  { 0 }
};

static void
test_parse_text (void)
{
  GPtrArray *items;

  static const gchar *text = "line-background=\"rgba(235,202,210,.4)\"\n"
                             "foreground=\"rgba(100%, 50%, 25%,.4)\"\n"
                             "color: #8d9091;\n"
                             "color: #123;\n"
                             "background-color: hsl(65, 70%, 72%);\n"
                             "text-shadow: 0 1px black;";

  printf ("\n");
  items = gstyle_color_parse (text);
  for (gint i = 0; i < items->len; ++i)
    {
      gchar *str;
      const GstyleColor *color;
      GstyleColorItem *item;
      guint start, len;

      item = g_ptr_array_index (items, i);
      color = gstyle_color_item_get_color (item);
      start = gstyle_color_item_get_start (item);
      len = gstyle_color_item_get_len (item);
      str = gstyle_color_to_string ((GstyleColor *)color, GSTYLE_COLOR_KIND_ORIGINAL);

      printf ("item(%i,%i): '%s'\n", start, len, str);
      g_free (str);
    }

  g_ptr_array_free (items, TRUE);
}

static void
test_parse_string (void)
{
  ColorItem *item;
  GstyleColor *color;
  GdkRGBA rgba;
  GstyleColorKind kind;
  GstyleCielab lab;

  printf ("\n");
  for (item = rgba_table; item->rgb != NULL; item++)
    {
      g_autofree gchar *str_hex3 = NULL;
      g_autofree gchar *str_hex6 = NULL;
      g_autofree gchar *str_rgba = NULL;
      g_autofree gchar *str_rgba_percent = NULL;
      g_autofree gchar *str_hsla = NULL;
      g_autofree gchar *str_original = NULL;
      g_autofree gchar *dst_str = NULL;

      color = gstyle_color_new_from_string (NULL, item->rgb);

      str_hex3 = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_HEX3);
      str_hex6 = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_HEX6);
      str_rgba = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGBA);
      str_rgba_percent = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGBA_PERCENT);
      str_hsla = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_HSLA);
      str_original = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_ORIGINAL);

      gstyle_color_fill_rgba (color, &rgba);
      gstyle_color_convert_rgb_to_cielab (&rgba, &lab);

      kind = gstyle_color_get_kind (color);
      dst_str = gstyle_color_to_string (color, kind);

      printf ("dst:'%s'\n", dst_str);


      printf ("\n----- '%s': rgba: kind:%i\n%s\n%s\n%s\n%s\n%s\n Original: %s\n",
              item->rgb, kind,
              str_hex3, str_hex6, str_rgba, str_rgba_percent, str_hsla, str_original);

      printf ("lab : L=%.3f a=%.3f b=%.3f\n", lab.l, lab.a, lab.b);

      // g_assert (g_ascii_strcasecmp (item->rgb, dst_str) == 0);
      g_object_unref (color);
      printf ("\n");
    }
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/parse_string", test_parse_string);
  g_test_add_func ("/Gstyle/parse_text", test_parse_text);

  return g_test_run ();
}
