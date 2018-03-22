/* ide-code-index-builder.h
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>

#include "ide-code-index-index.h"
#include "ide-code-index-service.h"

G_BEGIN_DECLS

#define IDE_TYPE_CODE_INDEX_BUILDER (ide_code_index_builder_get_type ())

G_DECLARE_FINAL_TYPE (IdeCodeIndexBuilder, ide_code_index_builder, IDE, CODE_INDEX_BUILDER, IdeObject)

IdeCodeIndexBuilder *ide_code_index_builder_new          (IdeContext           *context,
                                                          IdeCodeIndexService  *service,
                                                          IdeCodeIndexIndex    *index);
void                 ide_code_index_builder_drop_caches  (IdeCodeIndexBuilder  *self);
void                 ide_code_index_builder_build_async  (IdeCodeIndexBuilder  *self,
                                                          GFile                *directory,
                                                          gboolean              recursive,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
gboolean             ide_code_index_builder_build_finish (IdeCodeIndexBuilder  *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);

G_END_DECLS
