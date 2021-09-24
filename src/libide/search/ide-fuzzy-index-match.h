/* ide-fuzzy-index-match.h
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

G_BEGIN_DECLS

#define IDE_TYPE_FUZZY_INDEX_MATCH (ide_fuzzy_index_match_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFuzzyIndexMatch, ide_fuzzy_index_match, IDE, FUZZY_INDEX_MATCH, GObject)

IDE_AVAILABLE_IN_ALL
const gchar *ide_fuzzy_index_match_get_key      (IdeFuzzyIndexMatch *self);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_fuzzy_index_match_get_document (IdeFuzzyIndexMatch *self);
IDE_AVAILABLE_IN_ALL
gfloat       ide_fuzzy_index_match_get_score    (IdeFuzzyIndexMatch *self);
IDE_AVAILABLE_IN_ALL
guint        ide_fuzzy_index_match_get_priority (IdeFuzzyIndexMatch *self);

G_END_DECLS
