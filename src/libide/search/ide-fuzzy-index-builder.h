/* ide-fuzzy-index-builder.h
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

#define IDE_TYPE_FUZZY_INDEX_BUILDER (ide_fuzzy_index_builder_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFuzzyIndexBuilder, ide_fuzzy_index_builder, IDE, FUZZY_INDEX_BUILDER, GObject)

IDE_AVAILABLE_IN_ALL
IdeFuzzyIndexBuilder *ide_fuzzy_index_builder_new                 (void);
IDE_AVAILABLE_IN_ALL
gboolean              ide_fuzzy_index_builder_get_case_sensitive  (IdeFuzzyIndexBuilder  *self);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_set_case_sensitive  (IdeFuzzyIndexBuilder  *self,
                                                                   gboolean               case_sensitive);
IDE_AVAILABLE_IN_ALL
guint64               ide_fuzzy_index_builder_insert              (IdeFuzzyIndexBuilder  *self,
                                                                   const gchar           *key,
                                                                   GVariant              *document,
                                                                   guint                  priority);
IDE_AVAILABLE_IN_ALL
gboolean              ide_fuzzy_index_builder_write               (IdeFuzzyIndexBuilder  *self,
                                                                   GFile                 *file,
                                                                   gint                   io_priority,
                                                                   GCancellable          *cancellable,
                                                                   GError               **error);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_write_async         (IdeFuzzyIndexBuilder  *self,
                                                                   GFile                 *file,
                                                                   gint                   io_priority,
                                                                   GCancellable          *cancellable,
                                                                   GAsyncReadyCallback    callback,
                                                                   gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean              ide_fuzzy_index_builder_write_finish        (IdeFuzzyIndexBuilder  *self,
                                                                   GAsyncResult          *result,
                                                                   GError               **error);
IDE_AVAILABLE_IN_ALL
const GVariant       *ide_fuzzy_index_builder_get_document        (IdeFuzzyIndexBuilder  *self,
                                                                   guint64                document_id);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_set_metadata        (IdeFuzzyIndexBuilder  *self,
                                                                   const gchar           *key,
                                                                   GVariant              *value);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_set_metadata_string (IdeFuzzyIndexBuilder  *self,
                                                                   const gchar           *key,
                                                                   const gchar           *value);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_set_metadata_uint32 (IdeFuzzyIndexBuilder  *self,
                                                                   const gchar           *key,
                                                                   guint32                value);
IDE_AVAILABLE_IN_ALL
void                  ide_fuzzy_index_builder_set_metadata_uint64 (IdeFuzzyIndexBuilder  *self,
                                                                   const gchar           *key,
                                                                   guint64                value);

G_END_DECLS
