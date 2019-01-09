/* ide-xml-utils.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-utils.h"

static inline gboolean
is_name_start_char (gunichar ch)
{
  return ((ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') ||
          ch == ':' ||
          ch == '_' ||
          (ch >= 0xC0 && ch <= 0xD6) ||
          (ch >= 0xD8 && ch <= 0xF6) ||
          (ch >= 0xF8 && ch <= 0x2FF) ||
          (ch >= 0x370 && ch <= 0x37D) ||
          (ch >= 0x37F && ch <= 0x1FFF) ||
          (ch >= 0x200C && ch <= 0x200D) ||
          (ch >= 0x2070 && ch <= 0x218F) ||
          (ch >= 0x2C00 && ch <= 0x2FEF) ||
          (ch >= 0x3001 && ch <= 0xD7FF) ||
          (ch >= 0xF900 && ch <= 0xFDCF) ||
          (ch >= 0xFDF0 && ch <= 0xFFFD) ||
          (ch >= 0x10000 && ch <= 0xEFFFF));
}

static inline gboolean
is_name_char (gunichar ch)
{
  return (is_name_start_char (ch) ||
          ch == '-' ||
          ch == '.' ||
          (ch >= '0' && ch <= '9') ||
          ch == 0xB7 ||
          (ch >= 0x300 && ch <= 0x36F) ||
          (ch >= 0x203F && ch <= 0x2040));
}

/* Return TRUE if we found spaces */
static inline gboolean
skip_whitespaces (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_assert (cursor != NULL && *cursor != NULL);

  while ((ch = g_utf8_get_char (*cursor)) && g_unichar_isspace (ch))
    *cursor = g_utf8_next_char (*cursor);

  return (p != *cursor);
}

static void
jump_to_next_attribute (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;
  gchar term;
  gboolean has_spaces = FALSE;

  while ((ch = g_utf8_get_char (p)))
    {
      if (g_unichar_isspace (ch))
        {
          skip_whitespaces (&p);
          break;
        }

      if (ch == '=')
        break;

      p = g_utf8_next_char (p);
    }

  if (ch != '=')
    {
      *cursor = p;
      return;
    }

  p++;
  if (skip_whitespaces (&p))
    has_spaces = TRUE;

  ch = g_utf8_get_char (p);

  if (ch == '"' || ch == '\'')
    {
      term = ch;
      while ((ch = g_utf8_get_char (p)) && ch != term)
        p = g_utf8_next_char (p);

      if (ch == term)
        {
          p++;
          skip_whitespaces (&p);
        }
    }
  else if (!has_spaces)
    {
      while ((ch = g_utf8_get_char (p)) && !g_unichar_isspace (ch))
        p = g_utf8_next_char (p);
    }

  *cursor = p;
}

/* Return FALSE if not valid */
gboolean
ide_xml_utils_skip_element_name (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL, FALSE);

  if (!(ch = g_utf8_get_char (p)))
    return TRUE;

  if (!is_name_start_char (ch))
    return (g_unichar_isspace (ch));

  p = g_utf8_next_char (p);
  while ((ch = g_utf8_get_char (p)))
    {
      if (!is_name_char (ch))
        {
          *cursor = p;
          return g_unichar_isspace (ch);
        }

      p = g_utf8_next_char (p);
    }

  *cursor = p;
  return TRUE;
}

/* Return FALSE at the end of the string */
gboolean
ide_xml_utils_skip_attribute_value (const gchar **cursor,
                                    gchar         term)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL && *cursor != NULL, FALSE);

  while ((ch = g_utf8_get_char (p)) && ch != term)
    p = g_utf8_next_char (p);

  if (ch == term)
    p++;

  *cursor = p;
  return (ch != 0);
}

/* Return FALSE if not valid */
gboolean
ide_xml_utils_skip_attribute_name (const gchar **cursor)
{
  const gchar *p = *cursor;
  gunichar ch;

  g_return_val_if_fail (cursor != NULL, FALSE);

  if (!(ch = g_utf8_get_char (p)))
    return TRUE;

  if (!is_name_start_char (ch))
    {
      if (g_unichar_isspace (ch))
        return TRUE;

      *cursor = g_utf8_next_char (*cursor);
      return FALSE;
    }

  p = g_utf8_next_char (p);
  while ((ch = g_utf8_get_char (p)))
    {
      if (!is_name_char (ch))
        {
          *cursor = p;
          if (g_unichar_isspace (ch) || ch == '=')
            return TRUE;
          else
            {
              jump_to_next_attribute (cursor);
              return FALSE;
            }
        }

      p = g_utf8_next_char (p);
    }

  *cursor = p;
  return TRUE;
}
