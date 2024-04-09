/*
 * manuals-repository.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <gom/gom.h>
#include <libdex.h>

G_BEGIN_DECLS

#define MANUALS_TYPE_REPOSITORY (manuals_repository_get_type())

G_DECLARE_FINAL_TYPE (ManualsRepository, manuals_repository, MANUALS, REPOSITORY, GomRepository)

DexFuture  *manuals_repository_open                  (const char        *path);
DexFuture  *manuals_repository_close                 (ManualsRepository *self);
DexFuture  *manuals_repository_list                  (ManualsRepository *self,
                                                      GType              resource_type,
                                                      GomFilter         *filter);
DexFuture  *manuals_repository_list_sorted           (ManualsRepository *self,
                                                      GType              resource_type,
                                                      GomFilter         *filter,
                                                      GomSorting        *sorting);
DexFuture  *manuals_repository_count                 (ManualsRepository *self,
                                                      GType              resource_type,
                                                      GomFilter         *filter);
DexFuture  *manuals_repository_find_one              (ManualsRepository *self,
                                                      GType              resource_type,
                                                      GomFilter         *filter);
DexFuture  *manuals_repository_list_sdks             (ManualsRepository *self);
DexFuture  *manuals_repository_list_sdks_by_newest   (ManualsRepository *self);
DexFuture  *manuals_repository_delete                (ManualsRepository *self,
                                                      GType              resource_type,
                                                      GomFilter         *filter);
DexFuture  *manuals_repository_find_sdk              (ManualsRepository *self,
                                                      const char        *uri);
const char *manuals_repository_get_cached_book_title (ManualsRepository *self,
                                                      gint64             book_id);
const char *manuals_repository_get_cached_sdk_title  (ManualsRepository *self,
                                                      gint64             sdk_id);
gint64      manuals_repository_get_cached_sdk_id     (ManualsRepository *self,
                                                      gint64             book_id);

G_END_DECLS
