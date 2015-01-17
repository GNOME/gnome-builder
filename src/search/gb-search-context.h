/* gb-search-context.h
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

#ifndef GB_SEARCH_CONTEXT_H
#define GB_SEARCH_CONTEXT_H

#include <glib-object.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

#define GB_SEARCH_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_CONTEXT, GbSearchContext))
#define GB_SEARCH_CONTEXT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_CONTEXT, GbSearchContext const))
#define GB_SEARCH_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_CONTEXT, GbSearchContextClass))
#define GB_IS_SEARCH_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_CONTEXT))
#define GB_IS_SEARCH_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_CONTEXT))
#define GB_SEARCH_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_CONTEXT, GbSearchContextClass))

struct _GbSearchContext
{
  GObject parent;

  /*< private >*/
  GbSearchContextPrivate *priv;
};

struct _GbSearchContextClass
{
  GObjectClass parent;

  void (*result_added)   (GbSearchContext  *context,
                          GbSearchProvider *provider,
                          GbSearchResult   *result);
  void (*result_removed) (GbSearchContext  *context,
                          GbSearchProvider *provider,
                          GbSearchResult   *result);
};

GbSearchContext *gb_search_context_new                (void);
const GList     *gb_search_context_get_providers      (GbSearchContext  *context);
void             gb_search_context_add_provider       (GbSearchContext  *context,
                                                       GbSearchProvider *provider,
                                                       gsize             max_results);
void             gb_search_context_add_result         (GbSearchContext  *context,
                                                       GbSearchProvider *provider,
                                                       GbSearchResult   *result);
void             gb_search_context_remove_result      (GbSearchContext  *context,
                                                       GbSearchProvider *provider,
                                                       GbSearchResult   *result);
void             gb_search_context_cancel             (GbSearchContext  *context);
void             gb_search_context_execute            (GbSearchContext  *context,
                                                       const gchar      *search_terms);
void             gb_search_context_set_provider_count (GbSearchContext  *context,
                                                       GbSearchProvider *provider,
                                                       guint64           count);

G_END_DECLS

#endif /* GB_SEARCH_CONTEXT_H */
