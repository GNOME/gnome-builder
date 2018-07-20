/* ide-gi-radix-tree-macros.h
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

#pragma once

#include <glib.h>

#define NO_CAST_ALIGN_PUSH \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wcast-align\"")

#define NO_CAST_ALIGN_POP _Pragma("GCC diagnostic pop")

#define NO_CAST_SIZE_PUSH \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wint-to-pointer-cast\"")

#define NO_SIZE_ALIGN_POP _Pragma("GCC diagnostic pop")
typedef struct
{
  guint64 prefix_size : 8;
  guint64 nb_children : 8;
  guint64 nb_payloads : 8;
} NodeHeader;

G_STATIC_ASSERT (sizeof (NodeHeader) == sizeof (guint64));

typedef struct
{
  guint32 first_char : 8;
  guint32 offset     : 24;
} ChildHeader;

G_STATIC_ASSERT (sizeof (ChildHeader) == sizeof (guint32));
