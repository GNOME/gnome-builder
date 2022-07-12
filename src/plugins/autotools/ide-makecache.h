/* ide-makecache.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-foundry.h>

#include "ide-makecache-target.h"

G_BEGIN_DECLS

#define IDE_TYPE_MAKECACHE (ide_makecache_get_type())

G_DECLARE_FINAL_TYPE (IdeMakecache, ide_makecache, IDE, MAKECACHE, IdeObject)

void                 ide_makecache_new_for_cache_file_async  (IdeRuntime           *runtime,
                                                              IdePipeline          *pipeline,
                                                              GFile                *cache_file,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IdeMakecache        *ide_makecache_new_for_cache_file_finish (GAsyncResult         *result,
                                                              GError              **error);
void                 ide_makecache_get_file_flags_async      (IdeMakecache         *self,
                                                              GFile                *file,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
gchar              **ide_makecache_get_file_flags_finish     (IdeMakecache         *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                 ide_makecache_get_file_targets_async    (IdeMakecache         *self,
                                                              GFile                *file,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
GPtrArray           *ide_makecache_get_file_targets_finish   (IdeMakecache         *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                 ide_makecache_get_build_targets_async   (IdeMakecache         *self,
                                                              GFile                *build_dir,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
GPtrArray           *ide_makecache_get_build_targets_finish  (IdeMakecache         *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);

G_END_DECLS
