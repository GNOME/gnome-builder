/* ide-gi-formater.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-gi-utils.h"

#include "ide-gi-formater.h"

typedef enum
{
  FORMAT_STYLE_CODE,
  FORMAT_STYLE_TITLE,
  FORMAT_STYLE_TITLE_VALUE,
  FORMAT_STYLE_HEADER,
  FORMAT_STYLE_BODY,
  FORMAT_STYLE_FOOTER,
} FormatStyle;

typedef struct
{
  const gchar *prefix;
  const gchar *suffix;
} ElementStyle;

static ElementStyle html_styles [] =
{
    {"<pre class=\"programlisting\">", "</pre>"},  // FORMAT_STYLE_CODE
    {"<div class=\"title\"><h3>", "</h3></div>"},  // FORMAT_STYLE_TITLE
    {"<code class=\"literal\">", "</code>"},       // FORMAT_STYLE_TITLE_VALUE
    {"", ""},                                      // FORMAT_STYLE_HEADER
    {"<div class=\"content\">", "</div>"},         // FORMAT_STYLE_BODY
    {"", ""},                                      // FORMAT_STYLE_FOOTER
};

static ElementStyle * styles [] =
{
  html_styles,
};

static const gchar *
get_header (IdeGiFormaterType type)
{
  static const gchar *headers [] =
    {
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
      "  <html>\n"
      "    <head>\n"
      "      <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
      "    </head>\n"
      "    <body bgcolor=\"white\" text=\"black\" link=\"#0000FF\" vlink=\"#840084\" alink=\"#0000FF\">\n",
    };

  g_assert (type != IDE_GI_FORMATER_TYPE_TEXT);

  return headers [type];
}

static const gchar *
get_footer (IdeGiFormaterType type)
{
  static const gchar *footers [] =
    {
      "    </body>"
      "  </html>",
    };

  return footers [type];
}

static const gchar *css =
  "<style type=\"text/css\">"
  "body {margin: 0;padding: 0;}"
  "body * {font-size: 9pt;}"
  "a {text-decoration: none;}"
  ".title {font-size: 11pt;margin: 6pt;color: #a52a2a;}"
  ".title a {font-size: 11pt;color: #a52a2a;}"
  ".content {margin: 6pt;line-height: 1.3em;}"
  "p {margin-top: 0;margin-left: 2pt;}"
  "code {font-family: \"Bitstream Vera Sans Mono\", Monaco, Courier, monospace;font-size: 8pt;}"
  "pre.programlisting {font-family: \"Bitstream Vera Sans Mono\", Monaco, Courier, monospace;font-size: 8pt;"
  "padding: 6pt;background: #dddddd;-webkit-border-radius: 5px;overflow: hidden;}"
  "pre.programlisting a {font-family: \"Bitstream Vera Sans Mono\", Monaco, Courier, monospace;font-size: 8pt;}"
  "hr {display: none;}"
  "</style>";

static inline const gchar *
get_format_prefix (IdeGiFormaterType type,
                   FormatStyle       style)
{
  ElementStyle *style_entry;
  const gchar *prefix;

  g_assert (type != IDE_GI_FORMATER_TYPE_TEXT);

  style_entry = styles [type];
  prefix = style_entry [style].prefix;

  return prefix;
}

static inline const gchar *
get_format_suffix (IdeGiFormaterType type,
                   FormatStyle       style)
{
  ElementStyle *style_entry;
  const gchar *suffix;

  g_assert (type != IDE_GI_FORMATER_TYPE_TEXT);

  style_entry = styles [type];
  suffix = style_entry [style].suffix;

  return suffix;
}

/* Here we take ownership of str */
static GString *
format_text (GString           *str,
             IdeGiFormaterType  type,
             FormatStyle        style)
{
  const gchar *prefix = get_format_prefix (type, style);
  const gchar *suffix = get_format_suffix (type, style);

  g_string_prepend (str, prefix);
  g_string_append (str, suffix);

  return str;
}

static GString *
format_text_append_string (GString           *str,
                           GString           *string,
                           IdeGiFormaterType  type,
                           FormatStyle        style)
{
  const gchar *prefix = get_format_prefix (type, style);
  const gchar *suffix = get_format_suffix (type, style);

  g_assert (str != NULL);
  g_assert (string != NULL);

  g_string_append (str, prefix);
  g_string_append (str, string->str);
  g_string_append (str, suffix);

  return str;
}

/* If len = 0, a %NULL terminated C string is used */
static GString *
format_text_append_text (GString           *str,
                         const gchar       *text,
                         gsize              len,
                         IdeGiFormaterType  type,
                         FormatStyle        style)
{
  const gchar *prefix = get_format_prefix (type, style);
  const gchar *suffix = get_format_suffix (type, style);

  g_assert (str != NULL);
  g_assert (text != NULL);

  g_string_append (str, prefix);
  if (len > 0)
    g_string_append_len (str, text, len);
  else
    g_string_append (str, text);

  g_string_append (str, suffix);

  return str;
}

GString *
get_title (IdeGiBase         *base,
           IdeGiFormaterType  format_type)
{
  GString *title_str;
  IdeGiBlobType base_type;
  const gchar *base_name;
  g_autofree gchar *qname;

  title_str = g_string_new (NULL);

  base_type = ide_gi_base_get_object_type (base);
  base_name = ide_gi_utils_blob_type_to_string (base_type);

  qname = ide_gi_base_get_qualified_name (base);
  g_string_append (title_str, qname);
  format_text (title_str, format_type, FORMAT_STYLE_TITLE_VALUE);

  g_string_prepend (title_str, ": ");
  g_string_prepend (title_str, base_name + 1);
  g_string_prepend_unichar (title_str, g_ascii_toupper (*base_name));

  format_text (title_str, format_type, FORMAT_STYLE_TITLE);
  return title_str;
}

GString *
parse_body (const gchar       *body,
            IdeGiFormaterType  format_type)
{
  GString *body_str;
  const gchar *cursor = body;
  const gchar *prev = body;
  gsize body_len;

  body_len = strlen (body);
  body_str = g_string_sized_new (body_len);

  while (*body != '\0')
    {
      if ((cursor = strstr (body, "|[")))
        {
          const gchar *next;

          g_string_append_len (body_str, prev, cursor - prev);
          cursor += 2;

          if ((next = strstr (body, "]|")))
            {
              format_text_append_text (body_str, cursor, next - cursor, format_type, FORMAT_STYLE_CODE);
              next += 2;
              prev = body = next;
            }
          else
            {
              prev = body = cursor;
              continue;
            }
        }
      else
        {
          g_string_append_len (body_str, prev, body + body_len - prev);
          break;
        }
    }

  format_text (body_str, format_type, FORMAT_STYLE_BODY);
  return body_str;
}

gchar *
ide_gi_formater_get_doc (IdeGiBase         *base,
                         IdeGiFormaterType  format_type)
{
  GString *content = NULL;
  g_autoptr(IdeGiDoc) doc = NULL;
  g_autofree gchar *body = NULL;
  g_autofree gchar *title = NULL;

  content = g_string_new (NULL);
  if (base != NULL && (doc = ide_gi_base_get_doc (base)))
    {
      g_string_append (content, get_header (format_type));

      title = g_string_free (get_title (base, format_type), FALSE);
      g_string_append (content, title);

      body = g_string_free (parse_body (ide_gi_doc_get_doc (doc), format_type), FALSE);
      g_string_append (content, body);

      if (format_type == IDE_GI_FORMATER_TYPE_HTML)
        g_string_append (content, css);

      g_string_append (content, get_footer (format_type));
    }

  return g_string_free (content, FALSE);
}
