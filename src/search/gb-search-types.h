/* gb-search-types.h
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

#ifndef GB_SEARCH_TYPES_H
#define GB_SEARCH_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GB_TYPE_SEARCH_CONTEXT       (gb_search_context_get_type())
#define GB_TYPE_SEARCH_DISPLAY       (gb_search_display_get_type())
#define GB_TYPE_SEARCH_DISPLAY_GROUP (gb_search_display_group_get_type())
#define GB_TYPE_SEARCH_DISPLAY_ROW   (gb_search_display_row_get_type())
#define GB_TYPE_SEARCH_MANAGER       (gb_search_manager_get_type())
#define GB_TYPE_SEARCH_PROVIDER      (gb_search_provider_get_type())
#define GB_TYPE_SEARCH_RESULT        (gb_search_result_get_type())

typedef struct _GbSearchContext         GbSearchContext;
typedef struct _GbSearchContextClass    GbSearchContextClass;
typedef struct _GbSearchContextPrivate  GbSearchContextPrivate;

typedef struct _GbSearchDisplay        GbSearchDisplay;
typedef struct _GbSearchDisplayClass   GbSearchDisplayClass;
typedef struct _GbSearchDisplayPrivate GbSearchDisplayPrivate;

typedef struct _GbSearchDisplayGroup        GbSearchDisplayGroup;
typedef struct _GbSearchDisplayGroupClass   GbSearchDisplayGroupClass;
typedef struct _GbSearchDisplayGroupPrivate GbSearchDisplayGroupPrivate;

typedef struct _GbSearchDisplayRow        GbSearchDisplayRow;
typedef struct _GbSearchDisplayRowClass   GbSearchDisplayRowClass;
typedef struct _GbSearchDisplayRowPrivate GbSearchDisplayRowPrivate;

typedef struct _GbSearchProvider        GbSearchProvider;
typedef struct _GbSearchProviderClass   GbSearchProviderClass;
typedef struct _GbSearchProviderPrivate GbSearchProviderPrivate;

typedef struct _GbSearchManager         GbSearchManager;
typedef struct _GbSearchManagerClass    GbSearchManagerClass;
typedef struct _GbSearchManagerPrivate  GbSearchManagerPrivate;

typedef struct _GbSearchResult          GbSearchResult;
typedef struct _GbSearchResultClass     GbSearchResultClass;
typedef struct _GbSearchResultPrivate   GbSearchResultPrivate;

GType gb_search_context_get_type       (void);
GType gb_search_display_get_type       (void);
GType gb_search_display_group_get_type (void);
GType gb_search_display_row_get_type   (void);
GType gb_search_manager_get_type       (void);
GType gb_search_provider_get_type      (void);
GType gb_search_result_get_type        (void);

G_END_DECLS

#endif /* GB_SEARCH_TYPES_H */
