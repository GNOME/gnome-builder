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
 */

#pragma once

#include "ide-version-macros.h"

#include "ide-object.h"
#include "symbols/ide-symbol.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_ENTRY (ide_code_index_entry_get_type ())

G_DECLARE_DERIVABLE_TYPE (IdeCodeIndexEntry, ide_code_index_entry, IDE, CODE_INDEX_ENTRY, GObject)

struct _IdeCodeIndexEntryClass
{
  GObjectClass parent;

  /*< private */
  gpointer _padding[16];
};

IDE_AVAILABLE_IN_ALL
const gchar     *ide_code_index_entry_get_key   (IdeCodeIndexEntry *self);
IDE_AVAILABLE_IN_ALL
const gchar     *ide_code_index_entry_get_name  (IdeCodeIndexEntry *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolKind    ide_code_index_entry_get_kind  (IdeCodeIndexEntry *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolFlags   ide_code_index_entry_get_flags (IdeCodeIndexEntry *self);
IDE_AVAILABLE_IN_ALL
void             ide_code_index_entry_get_range (IdeCodeIndexEntry *self,
                                                 guint             *begin_line,
                                                 guint             *begin_line_offset,
                                                 guint             *end_line,
                                                 guint             *end_line_offset);

G_END_DECLS
