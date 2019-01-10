/* ide-source-style-scheme.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "ide-source-style-scheme.h"

gboolean
ide_source_style_scheme_apply_style (GtkSourceStyleScheme *style_scheme,
                                     const gchar          *style_name,
                                     GtkTextTag           *tag)
{
  g_autofree gchar *foreground = NULL;
  g_autofree gchar *background = NULL;
  g_autofree gchar *underline_color = NULL;
  GdkRGBA underline_rgba;
  GtkSourceStyle *style;
  const gchar *colon;
  PangoUnderline pango_underline;
  gboolean foreground_set = FALSE;
  gboolean background_set = FALSE;
  gboolean bold = FALSE;
  gboolean bold_set = FALSE;
  gboolean underline_set = FALSE;
  gboolean underline_color_set = FALSE;
  gboolean italic = FALSE;
  gboolean italic_set = FALSE;

  g_return_val_if_fail (!style_scheme || GTK_SOURCE_IS_STYLE_SCHEME (style_scheme), FALSE);
  g_return_val_if_fail (style_name != NULL, FALSE);

  g_object_set (tag,
                "foreground-set", FALSE,
                "background-set", FALSE,
                "weight-set", FALSE,
                "underline-set", FALSE,
                "underline-rgba-set", FALSE,
                "style-set", FALSE,
                NULL);

  if (style_scheme == NULL)
    return FALSE;

  style = gtk_source_style_scheme_get_style (style_scheme, style_name);

  if (style == NULL && (colon = strchr (style_name, ':')))
    {
      gchar defname[64];

      g_snprintf (defname, sizeof defname, "def%s", colon);

      style = gtk_source_style_scheme_get_style (style_scheme, defname);
    }

  if (style == NULL)
    return FALSE;

  g_object_get (style,
                "background", &background,
                "background-set", &background_set,
                "foreground", &foreground,
                "foreground-set", &foreground_set,
                "bold", &bold,
                "bold-set", &bold_set,
                "pango-underline", &pango_underline,
                "underline-set", &underline_set,
                "underline-color", &underline_color,
                "underline-color-set", &underline_color_set,
                "italic", &italic,
                "italic-set", &italic_set,
                NULL);

  if (background_set)
    g_object_set (tag, "background", background, NULL);

  if (foreground_set)
    g_object_set (tag, "foreground", foreground, NULL);

  if (bold_set && bold)
    g_object_set (tag, "weight", PANGO_WEIGHT_BOLD, NULL);

  if (italic_set && italic)
    g_object_set (tag, "style", PANGO_STYLE_ITALIC, NULL);

  if (underline_set)
    g_object_set (tag, "underline", pango_underline, NULL);

  if (underline_color_set && underline_color != NULL)
    {
      gdk_rgba_parse (&underline_rgba, underline_color);
      g_object_set (tag,
                    "underline-rgba", &underline_rgba,
                    NULL);
    }
  return TRUE;
}
