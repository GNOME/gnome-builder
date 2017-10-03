/* ide-code-indexer.h
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
#include "symbols/ide-code-index-entries.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEXER (ide_code_indexer_get_type())

G_DECLARE_INTERFACE (IdeCodeIndexer, ide_code_indexer, IDE, CODE_INDEXER, IdeObject)

struct _IdeCodeIndexerInterface
{
  GTypeInterface parent_iface;

  IdeCodeIndexEntries *(*index_file)             (IdeCodeIndexer       *self,
                                                  GFile                *file,
                                                  gchar               **build_flags,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
  void                 (*generate_key_async)     (IdeCodeIndexer       *self,
                                                  IdeSourceLocation    *location,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
  gchar               *(*generate_key_finish)    (IdeCodeIndexer       *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
};

IdeCodeIndexEntries  *ide_code_indexer_index_file           (IdeCodeIndexer       *self,
                                                             GFile                *file,
                                                             gchar               **build_flags,
                                                             GCancellable         *cancellable,
                                                             GError              **error);
void                  ide_code_indexer_generate_key_async   (IdeCodeIndexer       *self,
                                                             IdeSourceLocation    *location,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              user_data);
gchar                *ide_code_indexer_generate_key_finish  (IdeCodeIndexer       *self,
                                                             GAsyncResult         *result,
                                                             GError              **error);

G_END_DECLS
