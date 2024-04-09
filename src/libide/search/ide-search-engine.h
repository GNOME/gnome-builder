/* ide-search-engine.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_SEARCH_INSIDE) && !defined (IDE_SEARCH_COMPILATION)
# error "Only <libide-search.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-search-provider.h"
#include "ide-search-results.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_ENGINE (ide_search_engine_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSearchEngine, ide_search_engine, IDE, SEARCH_ENGINE, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeSearchEngine   *ide_search_engine_new                 (void);
IDE_AVAILABLE_IN_ALL
gboolean           ide_search_engine_get_busy            (IdeSearchEngine      *self);
IDE_AVAILABLE_IN_44
void               ide_search_engine_search_async        (IdeSearchEngine      *self,
                                                          IdeSearchCategory     category,
                                                          const char           *query,
                                                          guint                 max_results,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSearchResults  *ide_search_engine_search_finish       (IdeSearchEngine      *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
IDE_AVAILABLE_IN_ALL
void               ide_search_engine_add_provider        (IdeSearchEngine      *self,
                                                          IdeSearchProvider    *provider);
IDE_AVAILABLE_IN_ALL
void               ide_search_engine_remove_provider     (IdeSearchEngine      *self,
                                                          IdeSearchProvider    *provider);
IDE_AVAILABLE_IN_44
GListModel        *ide_search_engine_list_providers      (IdeSearchEngine      *self);
IDE_AVAILABLE_IN_47
IdeSearchProvider *ide_search_engine_find_by_module_name (IdeSearchEngine      *self,
                                                          const char           *module_name);

G_END_DECLS
