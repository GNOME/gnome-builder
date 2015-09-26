/* gb-string.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <string.h>

#include "gb-string.h"

gchar *
gb_str_highlight_full (const gchar     *str,
                       const gchar     *match,
                       gboolean         insensitive,
                       GbHighlightType  type)
{
  const gchar *begin = "<u>";
  const gchar *end = "</u>";
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;

  g_return_val_if_fail (str, NULL);
  g_return_val_if_fail (match, NULL);

  if (type == GB_HIGHLIGHT_BOLD)
    {
      begin = "<b>";
      end = "</b>";
    }

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if ((str_ch == match_ch) || (insensitive && (g_unichar_tolower (str_ch) == g_unichar_tolower (match_ch))))
        {
          g_string_append (ret, begin);
          g_string_append_unichar (ret, str_ch);
          g_string_append (ret, end);

          /*
           * TODO: We could seek to the next char and append in a batch.
           */
          match = g_utf8_next_char (match);
        }
      else
        {
          g_string_append_unichar (ret, str_ch);
        }
    }

  return g_string_free (ret, FALSE);
}

gchar *
gb_str_highlight (const gchar *str,
                  const gchar *match)
{
  return gb_str_highlight_full (str, match, FALSE, GB_HIGHLIGHT_BOLD);
}

gboolean
gb_str_simple_match (const gchar *haystack,
                     const gchar *needle_down)
{
  if (gb_str_empty0 (haystack))
    return FALSE;
  else if (gb_str_empty0 (needle_down))
    return TRUE;

  for (; *needle_down; needle_down = g_utf8_next_char (needle_down))
    {
      gunichar ch = g_utf8_get_char (needle_down);
      const gchar *tmp;

      tmp = strchr (haystack, ch);
      if (!tmp)
        tmp = strchr (haystack, g_unichar_toupper (ch));

      if (!tmp)
        return FALSE;

      haystack = tmp;
    }

  return TRUE;
}
