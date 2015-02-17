/* ide-search-reducer.h
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

#ifndef IDE_SEARCH_REDUCER_H
#define IDE_SEARCH_REDUCER_H

#include "ide-types.h"

G_BEGIN_DECLS

typedef struct
{
  IdeSearchContext  *context;
  IdeSearchProvider *provider;
  GSequence         *sequence;
  gsize              max_results;
  gsize              count;
} IdeSearchReducer;

void     ide_search_reducer_init    (IdeSearchReducer  *reducer,
                                     IdeSearchContext  *context,
                                     IdeSearchProvider *provider);
gboolean ide_search_reducer_accepts (IdeSearchReducer  *reducer,
                                     gfloat             score);
void     ide_search_reducer_push    (IdeSearchReducer  *reducer,
                                     IdeSearchResult   *result);
void     ide_search_reducer_destroy (IdeSearchReducer  *reducer);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (IdeSearchReducer, ide_search_reducer_destroy)

G_END_DECLS

#endif /* IDE_SEARCH_REDUCER_H */
