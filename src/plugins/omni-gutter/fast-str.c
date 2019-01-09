/* fast-str.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "fast-str"

#include "config.h"

#include <stdio.h>

#include "int-array.h"
#include "fast-str.h"

gint
_fast_str (guint         value,
           const gchar **strptr,
           gchar         alloc_buf[static 12])
{
  gint ret;

  /* The first offset is 10,000 so we can use that string but offset
   * ourselves into that string a bit to get the same number we would
   * if we had smaller strings available.
   */

  if (value < 10)
    {
      *strptr = int2str[value] + 4;
      return 1;
    }

  if (value < 100)
    {
      *strptr = int2str[value] + 3;
      return 2;
    }

  if (value < 1000)
    {
      *strptr = int2str[value] + 2;
      return 3;
    }

  if (value < 10000)
    {
      *strptr = int2str[value] + 1;
      return 4;
    }

  if (value < 20000)
    {
      *strptr = int2str[value - 10000];
      return 5;
    }

  *strptr = alloc_buf;
  ret = snprintf (alloc_buf, 12, "%u", value);
  alloc_buf[11] = 0;

  return ret;
}
