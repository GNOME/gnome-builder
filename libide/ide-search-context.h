/* ide-search-context.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_SEARCH_CONTEXT_H
#define IDE_SEARCH_CONTEXT_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_CONTEXT (ide_search_context_get_type())

G_DECLARE_FINAL_TYPE (IdeSearchContext, ide_search_context, IDE, SEARCH_CONTEXT, IdeObject)

const GList *ide_search_context_get_providers      (IdeSearchContext  *self);
void         ide_search_context_provider_completed (IdeSearchContext  *self,
                                                    IdeSearchProvider *provider);
void         ide_search_context_add_result         (IdeSearchContext  *self,
                                                    IdeSearchProvider *provider,
                                                    IdeSearchResult   *result);
void         ide_search_context_remove_result      (IdeSearchContext  *self,
                                                    IdeSearchProvider *provider,
                                                    IdeSearchResult   *result);
void         ide_search_context_cancel             (IdeSearchContext  *self);
void         ide_search_context_execute            (IdeSearchContext  *self,
                                                    const gchar       *search_terms,
                                                    gsize              max_results);
void         ide_search_context_set_provider_count (IdeSearchContext  *self,
                                                    IdeSearchProvider *provider,
                                                    guint64            count);
gsize        ide_search_context_get_max_results    (IdeSearchContext  *self);

G_END_DECLS

#endif /* IDE_SEARCH_CONTEXT_H */
