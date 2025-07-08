/*
 * gbp-css-preview-hover-provider.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-css-preview-hover-provider"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-editor.h>

#include "gbp-css-preview-hover-provider.h"

#define COLOR_FN_REGEX "(rgba?|hsla?)\\([0-9%]+(?:\\s*,\\s*[0-9%]+){2}(?:\\s*,\\s*[0-9]*\\.?[0-9]+)?\\s*\\)"
#define COLOR_UNS_FN_REGEX "(rgb|hsl|hwb|oklab|oklch|color)\\([^)]+\\)"
#define COLOR_HEX_REGEX "#[0-9a-fA-F]+"
#define GRADIENT_REGEX "(linear-gradient|radial-gradient|conic-gradient|repeating-linear-gradient|repeating-radial-gradient)\\s*\\((?:[^()]|\\([^)]*\\))*\\)"

static GRegex *color_fn_regex = NULL;
static GRegex *color_uns_fn_regex = NULL;
static GRegex *color_hex_regex = NULL;
static GRegex *gradient_regex = NULL;

static const struct {
  char name[32];
  char hex[8];
} css_colors[] = {
  {"aliceblue", "#f0f8ff"},
  {"antiquewhite", "#faebd7"},
  {"aqua", "#00ffff"},
  {"aquamarine", "#7fffd4"},
  {"azure", "#f0ffff"},
  {"beige", "#f5f5dc"},
  {"bisque", "#ffe4c4"},
  {"black", "#000000"},
  {"blanchedalmond", "#ffebcd"},
  {"blue", "#0000ff"},
  {"blueviolet", "#8a2be2"},
  {"brown", "#a52a2a"},
  {"burlywood", "#deb887"},
  {"cadetblue", "#5f9ea0"},
  {"chartreuse", "#7fff00"},
  {"chocolate", "#d2691e"},
  {"coral", "#ff7f50"},
  {"cornflowerblue", "#6495ed"},
  {"cornsilk", "#fff8dc"},
  {"crimson", "#dc143c"},
  {"cyan", "#00ffff"},
  {"darkblue", "#00008b"},
  {"darkcyan", "#008b8b"},
  {"darkgoldenrod", "#b8860b"},
  {"darkgray", "#a9a9a9"},
  {"darkgreen", "#006400"},
  {"darkgrey", "#a9a9a9"},
  {"darkkhaki", "#bdb76b"},
  {"darkmagenta", "#8b008b"},
  {"darkolivegreen", "#556b2f"},
  {"darkorange", "#ff8c00"},
  {"darkorchid", "#9932cc"},
  {"darkred", "#8b0000"},
  {"darksalmon", "#e9967a"},
  {"darkseagreen", "#8fbc8f"},
  {"darkslateblue", "#483d8b"},
  {"darkslategray", "#2f4f4f"},
  {"darkslategrey", "#2f4f4f"},
  {"darkturquoise", "#00ced1"},
  {"darkviolet", "#9400d3"},
  {"deeppink", "#ff1493"},
  {"deepskyblue", "#00bfff"},
  {"dimgray", "#696969"},
  {"dimgrey", "#696969"},
  {"dodgerblue", "#1e90ff"},
  {"firebrick", "#b22222"},
  {"floralwhite", "#fffaf0"},
  {"forestgreen", "#228b22"},
  {"fuchsia", "#ff00ff"},
  {"gainsboro", "#dcdcdc"},
  {"ghostwhite", "#f8f8ff"},
  {"gold", "#ffd700"},
  {"goldenrod", "#daa520"},
  {"gray", "#808080"},
  {"green", "#008000"},
  {"greenyellow", "#adff2f"},
  {"grey", "#808080"},
  {"honeydew", "#f0fff0"},
  {"hotpink", "#ff69b4"},
  {"indianred", "#cd5c5c"},
  {"indigo", "#4b0082"},
  {"ivory", "#fffff0"},
  {"khaki", "#f0e68c"},
  {"lavender", "#e6e6fa"},
  {"lavenderblush", "#fff0f5"},
  {"lawngreen", "#7cfc00"},
  {"lemonchiffon", "#fffacd"},
  {"lightblue", "#add8e6"},
  {"lightcoral", "#f08080"},
  {"lightcyan", "#e0ffff"},
  {"lightgoldenrodyellow", "#fafad2"},
  {"lightgreen", "#90ee90"},
  {"lightgrey", "#d3d3d3"},
  {"lightpink", "#ffb6c1"},
  {"lightsalmon", "#ffa07a"},
  {"lightseagreen", "#20b2aa"},
  {"lightskyblue", "#87cefa"},
  {"lightslategray", "#778899"},
  {"lightslategrey", "#778899"},
  {"lightsteelblue", "#b0c4de"},
  {"lightyellow", "#ffffe0"},
  {"lime", "#00ff00"},
  {"limegreen", "#32cd32"},
  {"linen", "#faf0e6"},
  {"magenta", "#ff00ff"},
  {"maroon", "#800000"},
  {"mediumaquamarine", "#66cdaa"},
  {"mediumblue", "#0000cd"},
  {"mediumorchid", "#ba55d3"},
  {"mediumpurple", "#9370db"},
  {"mediumseagreen", "#3cb371"},
  {"mediumslateblue", "#7b68ee"},
  {"mediumspringgreen", "#00fa9a"},
  {"mediumturquoise", "#48d1cc"},
  {"mediumvioletred", "#c71585"},
  {"midnightblue", "#191970"},
  {"mintcream", "#f5fffa"},
  {"mistyrose", "#ffe4e1"},
  {"moccasin", "#ffe4b5"},
  {"navajowhite", "#ffdead"},
  {"navy", "#000080"},
  {"oldlace", "#fdf5e6"},
  {"olive", "#808000"},
  {"olivedrab", "#6b8e23"},
  {"orange", "#ffa500"},
  {"orangered", "#ff4500"},
  {"orchid", "#da70d6"},
  {"palegoldenrod", "#eee8aa"},
  {"palegreen", "#98fb98"},
  {"paleturquoise", "#afeeee"},
  {"palevioletred", "#db7093"},
  {"papayawhip", "#ffefd5"},
  {"peachpuff", "#ffdab9"},
  {"peru", "#cd853f"},
  {"pink", "#ffc0cb"},
  {"plum", "#dda0dd"},
  {"powderblue", "#b0e0e6"},
  {"purple", "#800080"},
  {"red", "#ff0000"},
  {"rosybrown", "#bc8f8f"},
  {"royalblue", "#4169e1"},
  {"saddlebrown", "#8b4513"},
  {"salmon", "#fa8072"},
  {"sandybrown", "#f4a460"},
  {"seagreen", "#2e8b57"},
  {"seashell", "#fff5ee"},
  {"sienna", "#a0522d"},
  {"silver", "#c0c0c0"},
  {"skyblue", "#87ceeb"},
  {"slateblue", "#6a5acd"},
  {"slategray", "#708090"},
  {"slategrey", "#708090"},
  {"snow", "#fffafa"},
  {"springgreen", "#00ff7f"},
  {"steelblue", "#4682b4"},
  {"tan", "#d2b48c"},
  {"teal", "#008080"},
  {"thistle", "#d8bfd8"},
  {"tomato", "#ff6347"},
  {"turquoise", "#40e0d0"},
  {"violet", "#ee82ee"},
  {"wheat", "#f5deb3"},
  {"white", "#ffffff"},
  {"whitesmoke", "#f5f5f5"},
  {"yellow", "#ffff00"},
  {"yellowgreen", "#9acd32"}
};

struct _GbpCSSPreviewHoverProvider
{
  GObject parent_instance;

  char   *css_str;
  char   *description_str;
};

static gboolean
regex_match_text_to_cursor_position (GRegex     *regex,
                                     const char *text,
                                     guint       cursor_offset,
                                     char      **result)
{
  g_autoptr (GMatchInfo) match_info = NULL;

  *result = NULL;

  if (regex == NULL)
    return FALSE;

  if (g_regex_match (regex, text, 0, &match_info))
    {
      gint start_pos, end_pos;
      while (g_match_info_matches (match_info))
        {
          if (g_match_info_fetch_pos (match_info, 0, &start_pos, &end_pos))
            {
              if (cursor_offset >= start_pos && cursor_offset <= end_pos)
                {
                  if (g_set_str (result, g_match_info_fetch (match_info, 0)))
                    return TRUE;
                  return FALSE;
                }
            }
          g_match_info_next (match_info, NULL);
        }
    }
  return FALSE;
}

static char *
parse_css_gradient_with_description (const char *gradient_str)
{
  GString *desc = g_string_new ("");
  g_autofree char *str_copy = NULL;
  g_autofree char *content = NULL;
  g_autofree char *first_part = NULL;
  g_auto (GStrv) parts = NULL;
  char *start, *end;
  const char *direction_label = NULL;

  if (!gradient_str)
    return NULL;

  g_string_append_printf (desc, "<tt>%s</tt>\n", gradient_str);

  str_copy = g_strstrip (g_strdup (gradient_str));
  start = g_strstr_len (str_copy, -1, "(");
  end = g_strrstr (str_copy, ")");

  if (!start || !end || start >= end)
    return g_string_free (desc, FALSE);

  content = g_strstrip (g_strndup (start + 1, end - start - 1));
  parts = g_strsplit (content, ",", 2);

  if (!parts || !parts[0])
    return g_string_free (desc, FALSE);

  first_part = g_strstrip (g_strdup (parts[0]));

  if (g_str_has_prefix (str_copy, "linear"))
    {
      if (g_strrstr (first_part, "deg") ||
          g_strrstr (first_part, "rad") ||
          g_strrstr (first_part, "turn") ||
          g_strrstr (first_part, "grad"))
        {
          direction_label = _("Angle");
        }
      else if (g_str_has_prefix (first_part, "to "))
        {
          direction_label = _("Direction");
        }
    }
  else if (g_str_has_prefix (str_copy, "radial"))
    {
      if (g_strrstr (first_part, "circle") ||
          g_strrstr (first_part, "ellipse"))
        {
          direction_label = _("Shape");
        }
      else if (g_strrstr (first_part, "at "))
        {
          direction_label = _("Position");
        }
      else if (g_strrstr (first_part, "closest") ||
               g_strrstr (first_part, "farthest") ||
               g_strrstr (first_part, "px") ||
               g_strrstr (first_part, "%") ||
               g_strrstr (first_part, "em"))
        {
          direction_label = _("Size");
        }
    }
  else if (g_str_has_prefix (str_copy, "conic"))
    {
      if (g_strrstr (first_part, "from "))
        {
          direction_label = _("Angle");
        }
      else if (g_strrstr (first_part, "at "))
        {
          direction_label = _("Position");
        }
    }

  if (direction_label)
    {
      if (parts[1])
        {
          g_string_append_printf (desc, "<b>%s:</b> %s\n<b>%s:</b> %s",
                                  direction_label, first_part, _("Stops"), parts[1]);
        }
      else
        {
          g_string_append_printf (desc, "<b>%s:</b> %s", direction_label, first_part);
        }
    }
  else
    {
      if (parts[1])
        {
          g_string_append_printf (desc, "<b>%s:</b> %s, %s", _("Stops"), parts[0], parts[1]);
        }
      else
        {
          g_string_append_printf (desc, "<b>%s:</b> %s", _("Stops"), parts[0]);
        }
    }

  return g_string_free (desc, FALSE);
}

static char *
get_css_from_color (GdkRGBA *color)
{
  return g_strdup_printf ("* { background-color: rgba(%d, %d, %d, %.2f); }",
                          (int)(color->red * 255),
                          (int)(color->green * 255),
                          (int)(color->blue * 255),
                          color->alpha);
}

static char *
get_hex_from_color (GdkRGBA *color)
{
  if (color->alpha >= 1.0) {
    return g_strdup_printf ("#%02x%02x%02x",
                            (int)(color->red * 255),
                            (int)(color->green * 255),
                            (int)(color->blue * 255));
  } else {
    return g_strdup_printf ("#%02x%02x%02x%02x",
                            (int)(color->red * 255),
                            (int)(color->green * 255),
                            (int)(color->blue * 255),
                            (int)(color->alpha * 255));
  }
}

static gboolean
is_named_color (GdkRGBA *color,
                char   **color_name,
                char   **color_hex)
{
  g_autofree char *hex_string = get_hex_from_color (color);

  for (guint i = 0; i < G_N_ELEMENTS (css_colors); i++)
    {
      if (g_strcmp0(hex_string, css_colors[i].hex) == 0)
        {
          if (color_name)
            *color_name = g_strdup (css_colors[i].name);
          if (color_hex)
            *color_hex = g_strdup (hex_string);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
cursor_over_named_color (const char *text,
                         guint       cursor_offset,
                         char      **result,
                         GdkRGBA    *color)
{
  guint start, end;
  g_autofree char *word = NULL;
  g_autofree char *color_name = NULL;
  GdkRGBA parsed_color;

  if (!text || cursor_offset >= strlen (text))
    return FALSE;

  start = cursor_offset;
  end = cursor_offset;

  while (start > 0 && (g_ascii_isalnum (text[start - 1]) || text[start - 1] == '-' || text[start - 1] == '_'))
    start--;
  while (end < strlen (text) && (g_ascii_isalnum (text[end]) || text[end] == '-' || text[end] == '_'))
    end++;

  if (start == end)
    return FALSE;

  word = g_ascii_strdown (text + start, end - start);

  if (!gdk_rgba_parse (&parsed_color, word))
    return FALSE;

  if (is_named_color (&parsed_color, &color_name, NULL))
    {
      if (result)
        *result = g_strdup (word);
      if (color)
        *color = parsed_color;
      return TRUE;
    }

  return FALSE;
}

static char *
get_color_string (GdkRGBA *color) {
  g_auto (GStrv) parts = NULL;
  g_autofree char *color_str;

  color_str = gdk_rgba_to_string (color);

  parts = g_strsplit (color_str, ",", -1);

  return g_strjoinv (", ", parts);
}

static gboolean
extract_at_position (GbpCSSPreviewHoverProvider *self,
                     GtkTextBuffer              *buffer,
                     GtkTextIter                *iter)
{
  GtkTextIter start, end;
  g_autofree char *text = NULL;
  g_autofree char *result = NULL;
  guint cursor_offset;
  GdkRGBA color;

  start = *iter;
  end = *iter;

  for (int i = 0; i < 140 && gtk_text_iter_backward_char (&start); i++)
    {
      gunichar ch = gtk_text_iter_get_char (&start);
      if (ch == ';' || ch == '{' || ch == '}')
        {
          gtk_text_iter_forward_char (&start);
          break;
        }
    }

  for (int i = 0; i < 140 && gtk_text_iter_forward_char (&end); i++)
    {
      gunichar ch = gtk_text_iter_get_char (&end);
      if (ch == ';' || ch == '{' || ch == '}')
        {
          break;
        }
    }

  text = gtk_text_iter_get_text (&start, &end);
  if (!text || strlen (text) == 0)
    {
      return FALSE;
    }

  cursor_offset = gtk_text_iter_get_offset (iter) - gtk_text_iter_get_offset (&start);

  if (regex_match_text_to_cursor_position (color_fn_regex, text, cursor_offset, &result))
    {
      g_autofree char *color_name = NULL;

      if (!gdk_rgba_parse (&color, result))
        return FALSE;

      g_set_str (&self->css_str, get_css_from_color (&color));

      g_set_str (&self->description_str,
                 g_strdup_printf ("<tt>%s</tt>\n<tt>%s</tt>",
                                  result,
                                  get_hex_from_color (&color)));

      if (!g_str_has_prefix (result, "rgb"))
        {
          g_set_str (&self->description_str,
                     g_strdup_printf ("%s\n<tt>%s</tt>", self->description_str, get_color_string (&color)));
        }

      if (is_named_color (&color, &color_name, NULL))
        {
          g_set_str (&self->description_str,
                     g_strdup_printf ("%s\n<tt>%s</tt>", self->description_str, color_name));
        }

      return TRUE;
    }
  else if (regex_match_text_to_cursor_position (color_hex_regex, text, cursor_offset, &result))
    {
      g_autofree char *color_name = NULL;

      if (!gdk_rgba_parse (&color, result))
        return FALSE;

      g_set_str (&self->css_str, get_css_from_color (&color));

      g_set_str (&self->description_str,
                 g_strdup_printf ("<tt>%s</tt>\n<tt>%s</tt>",
                                  result,
                                  get_color_string (&color)));

      if (is_named_color (&color, &color_name, NULL))
        {
          g_set_str (&self->description_str,
                     g_strdup_printf ("%s\n<tt>%s</tt>", self->description_str, color_name));
        }

      return TRUE;
    }
  else if (cursor_over_named_color (text, cursor_offset, &result, &color))
    {
      g_set_str (&self->css_str, get_css_from_color (&color));

      g_set_str (&self->description_str,
                 g_strdup_printf ("<tt>%s</tt>\n<tt>%s</tt>\n<tt>%s</tt>",
                                  result,
                                  get_color_string (&color),
                                  get_hex_from_color (&color)));

      return TRUE;
    }
  else if (regex_match_text_to_cursor_position (color_uns_fn_regex, text, cursor_offset, &result))
    {
      g_set_str (&self->css_str,
                 g_strdup_printf ("* { background-color: %s; }", result));

      g_set_str (&self->description_str,
                 g_strdup_printf ("<tt>%s</tt>", result));

      return TRUE;
    }
  else if (regex_match_text_to_cursor_position (gradient_regex, text, cursor_offset, &result))
    {
      g_set_str (&self->css_str,
                 g_strdup_printf ("* { background: %s; }", result));

      g_set_str (&self->description_str,
                 parse_css_gradient_with_description (result));

      return TRUE;
    }

  return FALSE;
}

static void
gbp_css_preview_hover_provider_populate_async (GtkSourceHoverProvider *provider,
                                               GtkSourceHoverContext  *context,
                                               GtkSourceHoverDisplay  *display,
                                               GCancellable           *cancellable,
                                               GAsyncReadyCallback     callback,
                                               gpointer                user_data)
{
  GbpCSSPreviewHoverProvider *self = (GbpCSSPreviewHoverProvider *)provider;
  g_autoptr (GTask) task = NULL;
  GtkTextIter iter;
  GtkSourceBuffer *buffer;
  GtkBox *box;
  GtkWidget *color_box;
  GtkWidget *label;
  g_autofree char *css = NULL;
  g_autofree char *display_text = NULL;
  g_autofree char *color_text = NULL;
  g_autoptr (GtkCssProvider) css_provider = NULL;

  g_assert (GBP_IS_CSS_PREVIEW_HOVER_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_HOVER_CONTEXT (context));
  g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_css_preview_hover_provider_populate_async);

  gtk_source_hover_context_get_iter (context, &iter);
  buffer = gtk_source_hover_context_get_buffer (context);

  if (!extract_at_position (self, GTK_TEXT_BUFFER (buffer), &iter))
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "margin-top", 6,
                      "margin-bottom", 6,
                      NULL);

  color_box = g_object_new (GTK_TYPE_BOX,
                            "width-request", 60,
                            "height-request", 60,
                            "css-classes", IDE_STRV_INIT ("css-hover-preview-box"),
                            "vexpand", TRUE,
                            NULL);

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (css_provider, self->css_str);
  gtk_style_context_add_provider (gtk_widget_get_style_context (color_box),
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_box_append (box, color_box);

  if (self->description_str != NULL)
    {
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", self->description_str,
                            "use-markup", TRUE,
                            "xalign", 0.0f,
                            "yalign", 0.0f,
                            "margin-start", 12,
                            "selectable", TRUE,
                            NULL);

      gtk_box_append (box, label);
    }

  gtk_widget_add_css_class (GTK_WIDGET (box), "hover-display-row");
  gtk_source_hover_display_append (display, GTK_WIDGET (box));

  g_task_return_boolean (task, TRUE);
}

static gboolean
gbp_css_preview_hover_provider_populate_finish (GtkSourceHoverProvider  *provider,
                                                GAsyncResult            *result,
                                                GError                 **error)
{
  g_assert (GBP_IS_CSS_PREVIEW_HOVER_PROVIDER (provider));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
hover_provider_iface_init (GtkSourceHoverProviderInterface *iface)
{
  iface->populate_async = gbp_css_preview_hover_provider_populate_async;
  iface->populate_finish = gbp_css_preview_hover_provider_populate_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCSSPreviewHoverProvider, gbp_css_preview_hover_provider, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_HOVER_PROVIDER, hover_provider_iface_init))

static void
gbp_css_preview_hover_provider_finalize (GObject *object)
{
  GbpCSSPreviewHoverProvider *self = GBP_CSS_PREVIEW_HOVER_PROVIDER (object);

  g_clear_pointer (&self->css_str, g_free);
  g_clear_pointer (&self->description_str, g_free);

  G_OBJECT_CLASS (gbp_css_preview_hover_provider_parent_class)->finalize (object);
}

static void
gbp_css_preview_hover_provider_class_init (GbpCSSPreviewHoverProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gbp_css_preview_hover_provider_finalize;

  color_fn_regex = g_regex_new (COLOR_FN_REGEX, G_REGEX_OPTIMIZE, 0, NULL);
  color_uns_fn_regex = g_regex_new (COLOR_UNS_FN_REGEX, G_REGEX_OPTIMIZE, 0, NULL);
  color_hex_regex = g_regex_new (COLOR_HEX_REGEX, G_REGEX_OPTIMIZE, 0, NULL);
  gradient_regex = g_regex_new (GRADIENT_REGEX, G_REGEX_OPTIMIZE, 0, NULL);
}

static void
gbp_css_preview_hover_provider_init (GbpCSSPreviewHoverProvider *self)
{
}
