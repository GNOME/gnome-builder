/* ide-source-style-scheme.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-source-style-scheme"

#include "config.h"

#include <math.h>

#include "ide-source-style-scheme.h"

enum {
  FOREGROUND,
  BACKGROUND,
};

static gboolean
get_color (GtkSourceStyleScheme *scheme,
           const char           *style_name,
           GdkRGBA              *color,
           int                   kind)
{
  GtkSourceStyle *style;
  g_autofree char *fg = NULL;
  g_autofree char *bg = NULL;
  gboolean fg_set = FALSE;
  gboolean bg_set = FALSE;

  g_assert (GTK_SOURCE_IS_STYLE_SCHEME (scheme));
  g_assert (style_name != NULL);

  if (!(style = gtk_source_style_scheme_get_style (scheme, style_name)))
    return FALSE;

  g_object_get (style,
                "foreground", &fg,
                "foreground-set", &fg_set,
                "background", &bg,
                "background-set", &bg_set,
                NULL);

  if (kind == FOREGROUND && fg && fg_set)
    gdk_rgba_parse (color, fg);
  else if (kind == BACKGROUND && bg && bg_set)
    gdk_rgba_parse (color, bg);
  else
    return FALSE;

  return color->alpha >= .1;
}

static inline gboolean
get_background (GtkSourceStyleScheme *scheme,
                const char           *style_name,
                GdkRGBA              *bg)
{
  return get_color (scheme, style_name, bg, BACKGROUND);
}

gboolean
ide_source_style_scheme_is_dark (GtkSourceStyleScheme *scheme)
{
  const char *id;
  const char *variant;
  GdkRGBA text_bg;

  g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme), FALSE);

  id = gtk_source_style_scheme_get_id (scheme);
  variant = gtk_source_style_scheme_get_metadata (scheme, "variant");

  if (g_strcmp0 (variant, "light") == 0)
    return FALSE;
  else if (g_strcmp0 (variant, "dark") == 0)
    return TRUE;
  else if (strstr (id, "-dark") != NULL)
    return TRUE;

  if (get_background (scheme, "text", &text_bg))
    {
      /* http://alienryderflex.com/hsp.html */
      double r = text_bg.red * 255.0;
      double g = text_bg.green * 255.0;
      double b = text_bg.blue * 255.0;
      double hsp = sqrt (0.299 * (r * r) +
                         0.587 * (g * g) +
                         0.114 * (b * b));

      return hsp <= 127.5;
    }

  return FALSE;
}

/**
 * ide_source_style_scheme_get_variant:
 * @scheme: a #GtkSourceStyleScheme
 * @variant: the alternative variant
 *
 * Gets an alternate for a style scheme if one exists. Otherwise
 * @scheme is returned.
 *
 * Returns: (transfer none) (not nullable): a #GtkSourceStyleScheme
 */
GtkSourceStyleScheme *
ide_source_style_scheme_get_variant (GtkSourceStyleScheme *scheme,
                                     const char           *variant)
{
  GtkSourceStyleSchemeManager *style_scheme_manager;
  GtkSourceStyleScheme *ret;
  g_autoptr(GString) str = NULL;
  g_autofree char *key = NULL;
  const char *mapping;

  g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme), NULL);
  g_return_val_if_fail (g_strcmp0 (variant, "light") == 0 ||
                        g_strcmp0 (variant, "dark") == 0, NULL);

  style_scheme_manager = gtk_source_style_scheme_manager_get_default ();

  /* If the scheme provides "light-variant" or "dark-variant" metadata,
   * we will prefer those if the variant is available.
   */
  key = g_strdup_printf ("%s-variant", variant);
  if ((mapping = gtk_source_style_scheme_get_metadata (scheme, key)))
    {
      if ((ret = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, mapping)))
        return ret;
    }

  /* Try to find a match by replacing -light/-dark with @variant */
  str = g_string_new (gtk_source_style_scheme_get_id (scheme));

  if (g_str_has_suffix (str->str, "-light"))
    g_string_truncate (str, str->len - strlen ("-light"));
  else if (g_str_has_suffix (str->str, "-dark"))
    g_string_truncate (str, str->len - strlen ("-dark"));

  g_string_append_printf (str, "-%s", variant);

  /* Look for "Foo-variant" directly */
  if ((ret = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, str->str)))
    return ret;

  /* Look for "Foo" */
  g_string_truncate (str, str->len - strlen (variant) - 1);
  if ((ret = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, str->str)))
    return ret;

  /* Fallback to what we were provided */
  return scheme;
}

