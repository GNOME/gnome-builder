/* gb-search-reducer.h
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

#ifndef GB_SEARCH_REDUCER_H
#define GB_SEARCH_REDUCER_H

#include <glib.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

typedef struct
{
  GbSearchContext  *context;
  GbSearchProvider *provider;
  GSequence        *sequence;
  gsize             max_results;
  gsize             count;
} GbSearchReducer;

void     gb_search_reducer_init    (GbSearchReducer  *reducer,
                                    GbSearchContext  *context,
                                    GbSearchProvider *provider);
gboolean gb_search_reducer_accepts (GbSearchReducer  *reducer,
                                    gfloat            score);
void     gb_search_reducer_push    (GbSearchReducer  *reducer,
                                    GbSearchResult   *result);
void     gb_search_reducer_destroy (GbSearchReducer  *reducer);


G_END_DECLS

#endif /* GB_SEARCH_REDUCER_H */
