/* ide-code-index-entry.h
 *
 * Copyright (C) 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#ifndef IDE_CODE_INDEX_ENTRY_H
#define IDE_CODE_INDEX_ENTRY_H

#include "ide-object.h"
#include "ide-symbol.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_ENTRY (ide_code_index_entry_get_type ())

G_DECLARE_DERIVABLE_TYPE (IdeCodeIndexEntry, ide_code_index_entry, IDE, CODE_INDEX_ENTRY, GObject)

struct _IdeCodeIndexEntryClass
{
  GObjectClass parent;
};

gchar           *ide_code_index_entry_get_key   (IdeCodeIndexEntry *self);
gchar           *ide_code_index_entry_get_name  (IdeCodeIndexEntry *self);
IdeSymbolKind    ide_code_index_entry_get_kind  (IdeCodeIndexEntry *self);
IdeSymbolFlags   ide_code_index_entry_get_flags (IdeCodeIndexEntry *self);
void             ide_code_index_entry_get_range (IdeCodeIndexEntry *self,
                                                 guint             *begin_line,
                                                 guint             *begin_line_offset,
                                                 guint             *end_line,
                                                 guint             *end_line_offset);

G_END_DECLS

#endif /* IDE_CODE_INDEX_ENTRY_H */
