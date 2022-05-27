/* ide-code-index-entries.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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
# error "Only <libide-core.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_ENTRIES (ide_code_index_entries_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeCodeIndexEntries, ide_code_index_entries, IDE, CODE_INDEX_ENTRIES, GObject)

struct _IdeCodeIndexEntriesInterface
{
  GTypeInterface       parent_iface;

  GFile             *(*get_file)            (IdeCodeIndexEntries  *self);
  IdeCodeIndexEntry *(*get_next_entry)      (IdeCodeIndexEntries  *self);
  void               (*next_entries_async)  (IdeCodeIndexEntries  *self,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
  GPtrArray         *(*next_entries_finish) (IdeCodeIndexEntries  *self,
                                             GAsyncResult         *result,
                                             GError              **error);
};

IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntry *ide_code_index_entries_get_next_entry      (IdeCodeIndexEntries  *self);
IDE_AVAILABLE_IN_ALL
GFile             *ide_code_index_entries_get_file            (IdeCodeIndexEntries  *self);
IDE_AVAILABLE_IN_ALL
void               ide_code_index_entries_next_entries_async  (IdeCodeIndexEntries  *self,
                                                               GCancellable         *cancellable,
                                                               GAsyncReadyCallback   callback,
                                                               gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_code_index_entries_next_entries_finish (IdeCodeIndexEntries  *self,
                                                               GAsyncResult         *result,
                                                               GError              **error);
IDE_AVAILABLE_IN_ALL
void               ide_code_index_entries_collect_async       (IdeCodeIndexEntries  *self,
                                                               GCancellable         *cancellable,
                                                               GAsyncReadyCallback   callback,
                                                               gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_code_index_entries_collect_finish      (IdeCodeIndexEntries  *self,
                                                               GAsyncResult         *result,
                                                               GError              **error);

G_END_DECLS
