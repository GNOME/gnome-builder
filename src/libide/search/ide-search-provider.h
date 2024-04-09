/* ide-search-provider.h
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

#include "ide-search-preview.h"
#include "ide-search-result.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_PROVIDER (ide_search_provider_get_type())

typedef enum _IdeSearchCategory
{
  IDE_SEARCH_CATEGORY_EVERYTHING,
  IDE_SEARCH_CATEGORY_ACTIONS,
  IDE_SEARCH_CATEGORY_COMMANDS,
  IDE_SEARCH_CATEGORY_FILES,
  IDE_SEARCH_CATEGORY_SYMBOLS,
  IDE_SEARCH_CATEGORY_OTHER,
  IDE_SEARCH_CATEGORY_DOCUMENTATION,
} IdeSearchCategory;

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeSearchProvider, ide_search_provider, IDE, SEARCH_PROVIDER, IdeObject)

struct _IdeSearchProviderInterface
{
  GTypeInterface parent_interface;

  void               (*load)          (IdeSearchProvider    *self);
  void               (*unload)        (IdeSearchProvider    *self);
  void               (*search_async)  (IdeSearchProvider    *self,
                                       const gchar          *query,
                                       guint                 max_results,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  GListModel        *(*search_finish) (IdeSearchProvider    *self,
                                       GAsyncResult         *result,
                                       gboolean             *truncated,
                                       GError              **error);
  char              *(*dup_title)     (IdeSearchProvider    *self);
  GIcon             *(*dup_icon)      (IdeSearchProvider    *self);
  IdeSearchCategory  (*get_category)  (IdeSearchProvider    *self);
};

IDE_AVAILABLE_IN_ALL
void               ide_search_provider_load          (IdeSearchProvider    *self);
IDE_AVAILABLE_IN_ALL
void               ide_search_provider_unload        (IdeSearchProvider    *self);
IDE_AVAILABLE_IN_ALL
void               ide_search_provider_search_async  (IdeSearchProvider    *self,
                                                      const gchar          *query,
                                                      guint                 max_results,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GListModel        *ide_search_provider_search_finish (IdeSearchProvider    *self,
                                                      GAsyncResult         *result,
                                                      gboolean             *truncated,
                                                      GError              **error);
IDE_AVAILABLE_IN_44
IdeSearchCategory  ide_search_provider_get_category  (IdeSearchProvider    *self);
IDE_AVAILABLE_IN_44
char              *ide_search_provider_dup_title     (IdeSearchProvider     *self);
IDE_AVAILABLE_IN_44
GIcon             *ide_search_provider_dup_icon      (IdeSearchProvider     *self);

G_END_DECLS
