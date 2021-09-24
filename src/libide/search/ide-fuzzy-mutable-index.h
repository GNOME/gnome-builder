/* ide-fuzzy-mutable-index.h
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

#pragma once

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_FUZZY_MUTABLE_INDEX (ide_fuzzy_mutable_index_get_type())

typedef struct _IdeFuzzyMutableIndex IdeFuzzyMutableIndex;

typedef struct
{
   const char *key;
   gpointer    value;
   float       score;
   guint       id;
} IdeFuzzyMutableIndexMatch;

IDE_AVAILABLE_IN_ALL
GType                     ide_fuzzy_mutable_index_get_type           (void);
IDE_AVAILABLE_IN_ALL
IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_new                (gboolean              case_sensitive);
IDE_AVAILABLE_IN_ALL
IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_new_with_free_func (gboolean              case_sensitive,
                                                                      GDestroyNotify        free_func);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_set_free_func      (IdeFuzzyMutableIndex *fuzzy,
                                                                      GDestroyNotify        free_func);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_begin_bulk_insert  (IdeFuzzyMutableIndex *fuzzy);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_end_bulk_insert    (IdeFuzzyMutableIndex *fuzzy);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_fuzzy_mutable_index_contains           (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_insert             (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key,
                                                                      gpointer              value);
IDE_AVAILABLE_IN_ALL
GArray                   *ide_fuzzy_mutable_index_match              (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *needle,
                                                                      gsize                 max_matches);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_remove             (IdeFuzzyMutableIndex *fuzzy,
                                                                      const char           *key);
IDE_AVAILABLE_IN_ALL
IdeFuzzyMutableIndex     *ide_fuzzy_mutable_index_ref                (IdeFuzzyMutableIndex *fuzzy);
IDE_AVAILABLE_IN_ALL
void                      ide_fuzzy_mutable_index_unref              (IdeFuzzyMutableIndex *fuzzy);
IDE_AVAILABLE_IN_ALL
char                     *ide_fuzzy_highlight                        (const char           *str,
                                                                      const char           *query,
                                                                      gboolean              case_sensitive);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeFuzzyMutableIndex, ide_fuzzy_mutable_index_unref)

G_END_DECLS
