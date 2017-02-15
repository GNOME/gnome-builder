/* ide-build-utils.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <ide.h>

#include "ide-build-utils.h"

static void
skip_color_codes_values (const gchar **cursor)
{
  g_assert (cursor != NULL && *cursor != NULL);

  if (**cursor == 'm')
    {
      ++(*cursor);
      return;
    }

  while (**cursor != '\0')
    {
      while (**cursor >= '0' && **cursor <= '9')
        ++(*cursor);

      if (**cursor == ';')
        {
          ++(*cursor);
          continue;
        }

      if (**cursor == 'm')
        {
          ++(*cursor);
          break;
        }
    }
}

static gboolean
find_color_code (const gchar  *txt,
                 const gchar **start_offset,
                 const gchar **end_offset)
{
  const gchar *cursor = txt;

  g_assert (!ide_str_empty0 (txt));
  g_assert (start_offset != NULL);
  g_assert (end_offset != NULL);

  while (*cursor != '\0')
    {
      if (*cursor == '\\' && *(cursor + 1) == 'e')
        {
          *start_offset = cursor;
          cursor += 2;
        }
      else if (*cursor == '\033')
        {
          *start_offset = cursor;
          ++cursor;
        }
      else
        goto next;

      if (*cursor == '[')
        {
          ++cursor;
          if (*cursor == '\0')
            goto end;

          if (*cursor == 'K')
            {
              *end_offset = cursor + 1;
              return TRUE;
            }

          skip_color_codes_values (&cursor);
          *end_offset = cursor;

          return TRUE;
        }

      if (*cursor == '\0')
        goto end;

next:
      /* TODO: skip a possible escaped char */
      cursor = g_utf8_next_char (cursor);
    }

end:
  *start_offset = *end_offset = cursor;
  return FALSE;
}

gchar *
ide_build_utils_color_codes_filtering (const gchar *txt)
{
  const gchar *cursor = txt;
  const gchar *start_offset;
  const gchar *end_offset;
  GString *string;
  gsize len;
  gboolean ret;

  g_assert (txt != NULL);

  if (*txt == '\0')
    return g_strdup ("\0");

  string = g_string_new (NULL);

  while (*cursor != '\0')
    {
      ret = find_color_code (cursor, &start_offset, &end_offset);
      len = start_offset - cursor;
      if (len > 0)
        g_string_append_len (string, cursor, len);

      if (!ret)
        break;

      cursor = end_offset;
    }

  return g_string_free (string, FALSE);
}
