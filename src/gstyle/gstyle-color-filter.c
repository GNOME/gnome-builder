/* gstyle-color-filter.c
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

#include "gstyle-color-filter.h"

static gdouble web_colors [] = {0.0, 0.2, 0.2, 0.4, 0.4 , 0.6 , 0.6, 0.8, 0.8, 1.0, 1.0};
#define TO_WEB_COLOR(x) (web_colors [(gint)(x * 10.0)])

/* http://web.archive.org/web/20081014161121/http://www.colorjack.com/labs/colormatrix/ */
static gdouble blindness [][9] =
{
  /* achromatopsia */
  { 0.299, 0.587, 0.114,      0.299, 0.587, 0.114,       0.299, 0.587, 0.114   },
  /* achromatomaly */
  { 0.618, 0.32, 0.062,       0.163, 0.775, 0.062,       0.163, 0.32, 0.516    },
  /* deuteranopia */
  { 0.625, 0.375, 0.0,        0.7, 0.3, 0.0,             0.0, 0.3, 0.7         },
  /* deuteranomaly */
  { 0.80, 0.20, 0.0,          0.25833, 0.74167, 0.0,     0.0, 0.14167, 0.85833 },
  /* protanopia */
  { 0.56667, 0.43333, 0.0,    0.55833, 0.44167, 0.0,     0.0, 0.24167, 0.75833 },
  /* protanomaly */
  { 0.81667, 0.18333, 0.0,    0.33333, 0.66667, 0.0,     0.0, 0.125, 0.875     },
  /* tritanopia */
  { 0.950, 0.50, 0.0,         0.0, 0.43333, 0.56667,     0.0, 0.475, 0.525     },
  /* tritanomaly */
  { 0.96667, 0.3333, 0.0,     0.0, 0.73333, 0.26667,     0.0, 0.18333, 0.81667 },
};

typedef enum
{
  BLINDNESS_KIND_ACHROMATOPSIA,
  BLINDNESS_KIND_ACHROMATOMALY,
  BLINDNESS_KIND_DEUTERANOPIA,
  BLINDNESS_KIND_DEUTERANOMALY,
  BLINDNESS_KIND_PROTANOPIA,
  BLINDNESS_KIND_PROTANOMALY,
  BLINDNESS_KIND_TRITANOPIA,
  BLINDNESS_KIND_TRITANOMALY
} BlindnessKind;

static inline void
blindness_convert (GdkRGBA       *src_rgba,
                   GdkRGBA       *dst_rgba,
                   BlindnessKind  kind)
{
  GdkRGBA rgba;
  gdouble *base = &blindness [kind][0];

  rgba.red = src_rgba->red * base [0] + src_rgba->green * base [1] + src_rgba->blue * base [2];
  rgba.green = src_rgba->red * base [3] + src_rgba->green * base [4] + src_rgba->blue * base [5];
  rgba.blue = src_rgba->red * base [6] + src_rgba->green * base [7] + src_rgba->blue * base [8];

  if (rgba.red > 1.0)
    rgba.red = 1.0;

  if (rgba.green > 1.0)
    rgba.green = 1.0;

  if (rgba.blue > 1.0)
    rgba.blue = 1.0;

  rgba.alpha = src_rgba->alpha;
  *dst_rgba = rgba;
}

/**
 * gstyle_color_filter_websafe:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A WebSafe color filter usable with #GstyleColorScale and GstyleColorPlane.
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

/**
 * gstyle_color_filter_achromatopsia:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A achromatopsia (color agnosia) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_achromatopsia (GdkRGBA  *rgba,
                                   GdkRGBA  *filter_rgba,
                                   gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_ACHROMATOPSIA);
}

/**
 * gstyle_color_filter_achromatomaly:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A achromatomaly (Blue Cone Monochromacy) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_achromatomaly (GdkRGBA  *rgba,
                                   GdkRGBA  *filter_rgba,
                                   gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_ACHROMATOMALY);
}

/**
 * gstyle_color_filter_deuteranopia:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A deuteranopia (green-blind) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_deuteranopia (GdkRGBA  *rgba,
                                  GdkRGBA  *filter_rgba,
                                  gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_DEUTERANOPIA);
}

/**
 * gstyle_color_filter_deuteranomaly:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A deuteranomaly (green-weak) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_deuteranomaly (GdkRGBA  *rgba,
                                   GdkRGBA  *filter_rgba,
                                   gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_DEUTERANOMALY);
}

/**
 * gstyle_color_filter_protanopia:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A protanopia (red-blind) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_protanopia (GdkRGBA  *rgba,
                                GdkRGBA  *filter_rgba,
                                gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_PROTANOPIA);
}

/**
 * gstyle_color_filter_protanomaly:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A protanomaly (red-weak) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_protanomaly (GdkRGBA  *rgba,
                                 GdkRGBA  *filter_rgba,
                                 gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_PROTANOMALY);
}

/**
 * gstyle_color_filter_tritanopia:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A tritanopia (blue-blind) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_tritanopia (GdkRGBA  *rgba,
                                GdkRGBA  *filter_rgba,
                                gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_TRITANOPIA);
}

/**
 * gstyle_color_filter_tritanomaly:
 * @rgba: The source #GdkRGBA color
 * @filter_rgba: (out): the filtered #GdkRGBA color
 * @user_data: A user data pointer
 *
 * A tritanomaly (blue-weak) color filter usable with #GstyleColorScale and GstyleColorPlane.
 *
 */
void
gstyle_color_filter_tritanomaly (GdkRGBA  *rgba,
                                 GdkRGBA  *filter_rgba,
                                 gpointer  user_data)
{
  blindness_convert (rgba, filter_rgba, BLINDNESS_KIND_TRITANOMALY);
}

GType
gstyle_color_filter_get_type (void)
{
  static GType filter_type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_FILTER_NONE,          "GSTYLE_COLOR_FILTER_NONE",          "none" },
    { GSTYLE_COLOR_FILTER_ACHROMATOPSIA, "GSTYLE_COLOR_FILTER_ACHROMATOPSIA", "achromatopsia" },
    { GSTYLE_COLOR_FILTER_ACHROMATOMALY, "GSTYLE_COLOR_FILTER_ACHROMATOMALY", "achromatomaly" },
    { GSTYLE_COLOR_FILTER_DEUTERANOPIA,  "GSTYLE_COLOR_FILTER_DEUTERANOPIA",  "deuteranopia" },
    { GSTYLE_COLOR_FILTER_DEUTERANOMALY, "GSTYLE_COLOR_FILTER_DEUTERANOMALY", "deuteranomaly" },
    { GSTYLE_COLOR_FILTER_PROTANOPIA,    "GSTYLE_COLOR_FILTER_PROTANOPIA",    "protanopia" },
    { GSTYLE_COLOR_FILTER_PROTANOMALY,   "GSTYLE_COLOR_FILTER_PROTANOMALY",   "protanomaly" },
    { GSTYLE_COLOR_FILTER_TRITANOPIA,    "GSTYLE_COLOR_FILTER_TRITANOPIA",    "tritanopia" },
    { GSTYLE_COLOR_FILTER_TRITANOMALY,   "GSTYLE_COLOR_FILTER_TRITANOMALY",   "tritanomaly" },
    { GSTYLE_COLOR_FILTER_WEBSAFE,       "GSTYLE_COLOR_FILTER_WEBSAFE",       "websafe" },
    { 0 }
  };

  if (g_once_init_enter (&filter_type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorFilter", values);
      g_once_init_leave (&filter_type_id, _type_id);
    }

  return filter_type_id;
}
