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

#include "gb-string.h"

gchar *
gb_str_highlight (const gchar *str,
                  const gchar *match)
{
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;

  g_return_val_if_fail (str, NULL);
  g_return_val_if_fail (match, NULL);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if (str_ch == match_ch)
        {
          g_string_append (ret, "<u>");
          g_string_append_unichar (ret, str_ch);
          g_string_append (ret, "</u>");

          match = g_utf8_next_char (match);
        }
      else
        {
          g_string_append_unichar (ret, str_ch);
        }
    }

  return g_string_free (ret, FALSE);
}
