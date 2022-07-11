/* ide-fuzzy-index-cursor.h
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <libide-core.h>

#include "ide-fuzzy-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_FUZZY_INDEX_CURSOR (ide_fuzzy_index_cursor_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFuzzyIndexCursor, ide_fuzzy_index_cursor, IDE, FUZZY_INDEX_CURSOR, GObject)

IDE_AVAILABLE_IN_ALL
IdeFuzzyIndex *ide_fuzzy_index_cursor_get_index (IdeFuzzyIndexCursor *self);

G_END_DECLS
