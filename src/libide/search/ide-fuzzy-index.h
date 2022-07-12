/* ide-fuzzy-index.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define IDE_TYPE_FUZZY_INDEX (ide_fuzzy_index_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFuzzyIndex, ide_fuzzy_index, IDE, FUZZY_INDEX, GObject)

IDE_AVAILABLE_IN_ALL
IdeFuzzyIndex  *ide_fuzzy_index_new                 (void);
IDE_AVAILABLE_IN_ALL
gboolean        ide_fuzzy_index_load_file           (IdeFuzzyIndex        *self,
                                                     GFile                *file,
                                                     GCancellable         *cancellable,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_fuzzy_index_load_file_async     (IdeFuzzyIndex        *self,
                                                     GFile                *file,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean        ide_fuzzy_index_load_file_finish    (IdeFuzzyIndex        *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void            ide_fuzzy_index_query_async         (IdeFuzzyIndex        *self,
                                                     const gchar          *query,
                                                     guint                 max_matches,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel     *ide_fuzzy_index_query_finish        (IdeFuzzyIndex        *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
GVariant       *ide_fuzzy_index_get_metadata        (IdeFuzzyIndex        *self,
                                                     const gchar          *key);
IDE_AVAILABLE_IN_ALL
guint32         ide_fuzzy_index_get_metadata_uint32 (IdeFuzzyIndex        *self,
                                                     const gchar          *key);
IDE_AVAILABLE_IN_ALL
guint64         ide_fuzzy_index_get_metadata_uint64 (IdeFuzzyIndex        *self,
                                                     const gchar          *key);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_fuzzy_index_get_metadata_string (IdeFuzzyIndex        *self,
                                                     const gchar          *key);

G_END_DECLS
