/* gb-pango.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gstdio.h>

#include "gb-pango.h"

static void
_add_keyval (GString     *str,
             const gchar *key,
             const gchar *value)
{
  g_string_append_printf (str, "%s:%s;", key, value);
}

gchar *
gb_pango_font_description_to_css (const PangoFontDescription *font_desc)
{
  PangoFontMask mask;
  GString *str;

  g_return_val_if_fail (font_desc, NULL);

  str = g_string_new (NULL);

  mask = pango_font_description_get_set_fields (font_desc);

  if ((mask & PANGO_FONT_MASK_FAMILY) != 0)
    {
      const gchar *family;

      family = pango_font_description_get_family (font_desc);
      _add_keyval (str, "font-family", family);
    }

  if ((mask & PANGO_FONT_MASK_STYLE) != 0)
    {
      PangoVariant variant;

      variant = pango_font_description_get_variant (font_desc);

      switch (variant)
        {
        case PANGO_VARIANT_NORMAL:
          _add_keyval (str, "font-variant", "normal");
          break;

        case PANGO_VARIANT_SMALL_CAPS:
          _add_keyval (str, "font-variant", "small-caps");
          break;

        default:
          break;
        }
    }

  if ((mask & PANGO_FONT_MASK_WEIGHT))
    {
      gchar weight[12];

      g_snprintf (weight, sizeof weight, "%d",
                  (int)pango_font_description_get_weight (font_desc));
      _add_keyval (str, "font-weight", weight);
    }

  if ((mask & PANGO_FONT_MASK_STRETCH))
    {
      const gchar *key = "font-stretch";

      switch (pango_font_description_get_stretch (font_desc))
        {
        case PANGO_STRETCH_ULTRA_CONDENSED:
          _add_keyval (str, key, "untra-condensed");
          break;

        case PANGO_STRETCH_EXTRA_CONDENSED:
          _add_keyval (str, key, "extra-condensed");
          break;

        case PANGO_STRETCH_CONDENSED:
          _add_keyval (str, key, "condensed");
          break;

        case PANGO_STRETCH_SEMI_CONDENSED:
          _add_keyval (str, key, "semi-condensed");
          break;

        case PANGO_STRETCH_NORMAL:
          _add_keyval (str, key, "normal");
          break;

        case PANGO_STRETCH_SEMI_EXPANDED:
          _add_keyval (str, key, "semi-expanded");
          break;

        case PANGO_STRETCH_EXPANDED:
          _add_keyval (str, key, "expanded");
          break;

        case PANGO_STRETCH_EXTRA_EXPANDED:
          _add_keyval (str, key, "extra-expanded");
          break;

        case PANGO_STRETCH_ULTRA_EXPANDED:
          _add_keyval (str, key, "untra-expanded");
          break;

        default:
          break;
        }
    }

  if ((mask & PANGO_FONT_MASK_SIZE))
    {
      gint font_size;
      gchar value[12];

      font_size = pango_font_description_get_size (font_desc) / PANGO_SCALE;
      g_snprintf (value, sizeof value, "%dpx", font_size);
      _add_keyval (str, "font-size", value);
    }

  return g_string_free (str, FALSE);
}
