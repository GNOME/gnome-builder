/* ide-code-index-entries.h
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

#pragma once

#include "ide-object.h"
#include "symbols/ide-symbol.h"
#include "symbols/ide-code-index-entry.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_ENTRIES (ide_code_index_entries_get_type ())

G_DECLARE_INTERFACE (IdeCodeIndexEntries, ide_code_index_entries, IDE, CODE_INDEX_ENTRIES, GObject)

struct _IdeCodeIndexEntriesInterface
{
  GTypeInterface parent_iface;

  IdeCodeIndexEntry   *(*get_next_entry)   (IdeCodeIndexEntries *self);
};

IdeCodeIndexEntry  *ide_code_index_entries_get_next_entry (IdeCodeIndexEntries *self);

G_END_DECLS
