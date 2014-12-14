/* gb-search-types.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SEARCH_TYPES_H
#define GB_SEARCH_TYPES_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GbSearchContext           GbSearchContext;
typedef struct _GbSearchContextClass      GbSearchContextClass;
typedef struct _GbSearchContextPrivate    GbSearchContextPrivate;

typedef struct _GbSearchProvider          GbSearchProvider;
typedef struct _GbSearchProviderInterface GbSearchProviderInterface;

typedef struct _GbSearchResult            GbSearchResult;
typedef struct _GbSearchResultClass       GbSearchResultClass;
typedef struct _GbSearchResultPrivate     GbSearchResultPrivate;

G_END_DECLS

#endif /* GB_SEARCH_TYPES_H */
