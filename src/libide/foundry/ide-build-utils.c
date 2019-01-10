/* ide-build-utils.c
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

#define G_LOG_DOMAIN "ide-build-utils"

#include "config.h"

#include "ide-build-private.h"

guint8 *
_ide_build_utils_filter_color_codes (const guint8 *data,
                                     gsize         len,
                                     gsize        *out_len)
{
  g_autoptr(GByteArray) dst = NULL;

  g_return_val_if_fail (out_len != NULL, NULL);

  *out_len = 0;

  if (data == NULL)
    return NULL;
  else if (len == 0)
    return (guint8 *)g_strdup ("");

  dst = g_byte_array_sized_new (len);

  for (gsize i = 0; i < len; i++)
    {
      guint8 ch = data[i];
      guint8 next = (i+1) < len ? data[i+1] : 0;

      if (ch == '\\' && next == 'e')
        {
          i += 2;
        }
      else if (ch == '\033')
        {
          i++;
        }
      else
        {
          g_byte_array_append (dst, &ch, 1);
          continue;
        }

      if (i >= len)
        break;

      if (data[i] == '[')
        i++;

      if (i >= len)
        break;

      for (; i < len; i++)
        {
          ch = data[i];

          if (g_ascii_isdigit (ch) || ch == ' ' || ch == ';')
            continue;

          break;
        }
    }

  *out_len = dst->len;

  return g_byte_array_free (g_steal_pointer (&dst), FALSE);
}
