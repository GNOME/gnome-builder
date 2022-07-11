/* ide-line-reader.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-core.h>

G_BEGIN_DECLS

typedef struct
{
  gchar  *contents;
  gsize   length;
  gssize  pos;
} IdeLineReader;

IDE_AVAILABLE_IN_ALL
void   ide_line_reader_init (IdeLineReader *reader,
                             gchar         *contents,
                             gssize         length);
IDE_AVAILABLE_IN_ALL
gchar *ide_line_reader_next (IdeLineReader *reader,
                             gsize         *length);

G_END_DECLS
