/* ide-code-indexer.h
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

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEXER (ide_code_indexer_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeCodeIndexer, ide_code_indexer, IDE, CODE_INDEXER, IdeObject)

struct _IdeCodeIndexerInterface
{
  GTypeInterface         parent_iface;

  void                 (*generate_key_async)     (IdeCodeIndexer       *self,
                                                  IdeLocation          *location,
                                                  const gchar * const  *build_flags,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
  gchar               *(*generate_key_finish)    (IdeCodeIndexer       *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
  void                 (*index_file_async)       (IdeCodeIndexer       *self,
                                                  GFile                *file,
                                                  const gchar * const  *build_flags,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
  IdeCodeIndexEntries *(*index_file_finish)      (IdeCodeIndexer       *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
};

IDE_AVAILABLE_IN_ALL
void                 ide_code_indexer_index_file_async    (IdeCodeIndexer       *self,
                                                           GFile                *file,
                                                           const gchar * const  *build_flags,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeCodeIndexEntries *ide_code_indexer_index_file_finish   (IdeCodeIndexer       *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
IDE_AVAILABLE_IN_ALL
void                 ide_code_indexer_generate_key_async  (IdeCodeIndexer       *self,
                                                           IdeLocation          *location,
                                                           const gchar * const  *build_flags,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gchar               *ide_code_indexer_generate_key_finish (IdeCodeIndexer       *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);

G_END_DECLS
