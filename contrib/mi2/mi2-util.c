/* mi2-util.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "mi2-util.h"

gchar *
mi2_util_parse_string (const gchar  *line,
                       const gchar **endptr)
{
  g_autoptr(GString) str = NULL;

  g_return_val_if_fail (line != NULL, NULL);

  if (*line != '"')
    goto failure;

  str = g_string_new (NULL);

  for (++line; *line; line = g_utf8_next_char (line))
    {
      gunichar ch = g_utf8_get_char (line);

      if (ch == '"')
        break;

      /* Handle escape characters */
      if (ch == '\\')
        {
          line = g_utf8_next_char (line);
          if (!*line)
            goto failure;
          ch = g_utf8_get_char (line);

          switch (ch)
            {
            case 'n':
              g_string_append (str, "\n");
              break;

            case 't':
              g_string_append (str, "\t");
              break;

            default:
              g_string_append_unichar (str, ch);
              break;
            }

          continue;
        }

      g_string_append_unichar (str, ch);
    }

  if (*line == '"')
    line++;

  if (endptr)
    *endptr = line;

  return g_string_free (g_steal_pointer (&str), FALSE);

failure:
  if (endptr)
    *endptr = NULL;

  return NULL;
}

gchar *
mi2_util_parse_word (const gchar  *line,
                     const gchar **endptr)
{
  const gchar *begin = line;
  gchar *ret;

  g_return_val_if_fail (line != NULL, NULL);

  for (; *line; line = g_utf8_next_char (line))
    {
      gunichar ch = g_utf8_get_char (line);

      if (ch == ',' || ch == '=' || g_unichar_isspace (ch))
        break;
    }

  ret = g_strndup (begin, line - begin);

  if (*line)
    line++;

  if (endptr)
    *endptr = line;

  return ret;
}
