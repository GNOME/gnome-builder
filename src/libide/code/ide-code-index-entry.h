/* ide-code-index-entry.h
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"
#include "ide-symbol.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_ENTRY         (ide_code_index_entry_get_type())
#define IDE_TYPE_CODE_INDEX_ENTRY_BUILDER (ide_code_index_entry_builder_get_type())

typedef struct _IdeCodeIndexEntry        IdeCodeIndexEntry;
typedef struct _IdeCodeIndexEntryBuilder IdeCodeIndexEntryBuilder;

IDE_AVAILABLE_IN_ALL
GType                     ide_code_index_entry_get_type          (void);
IDE_AVAILABLE_IN_ALL
GType                     ide_code_index_entry_builder_get_type  (void);
IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntryBuilder *ide_code_index_entry_builder_new       (void);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_set_range (IdeCodeIndexEntryBuilder *builder,
                                                                  guint                     begin_line,
                                                                  guint                     begin_line_offset,
                                                                  guint                     end_line,
                                                                  guint                     end_line_offset);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_set_key   (IdeCodeIndexEntryBuilder *builder,
                                                                  const gchar              *key);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_set_name  (IdeCodeIndexEntryBuilder *builder,
                                                                  const gchar              *name);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_set_kind  (IdeCodeIndexEntryBuilder *builder,
                                                                  IdeSymbolKind             kind);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_set_flags (IdeCodeIndexEntryBuilder *builder,
                                                                  IdeSymbolFlags            flags);
IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntry        *ide_code_index_entry_builder_build     (IdeCodeIndexEntryBuilder *builder);
IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntryBuilder *ide_code_index_entry_builder_copy      (IdeCodeIndexEntryBuilder *builder);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_builder_free      (IdeCodeIndexEntryBuilder *builder);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_free              (IdeCodeIndexEntry        *self);
IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntry        *ide_code_index_entry_copy              (const IdeCodeIndexEntry  *self);
IDE_AVAILABLE_IN_ALL
const gchar              *ide_code_index_entry_get_key           (const IdeCodeIndexEntry  *self);
IDE_AVAILABLE_IN_ALL
const gchar              *ide_code_index_entry_get_name          (const IdeCodeIndexEntry  *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolKind             ide_code_index_entry_get_kind          (const IdeCodeIndexEntry  *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolFlags            ide_code_index_entry_get_flags         (const IdeCodeIndexEntry  *self);
IDE_AVAILABLE_IN_ALL
void                      ide_code_index_entry_get_range         (const IdeCodeIndexEntry  *self,
                                                                  guint                    *begin_line,
                                                                  guint                    *begin_line_offset,
                                                                  guint                    *end_line,
                                                                  guint                    *end_line_offset);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeCodeIndexEntry, ide_code_index_entry_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeCodeIndexEntryBuilder, ide_code_index_entry_builder_free)

G_END_DECLS
