/* gb-search-provider.h
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

#ifndef GB_SEARCH_PROVIDER_H
#define GB_SEARCH_PROVIDER_H

#include <gio/gio.h>

#include "gb-search-context.h"

G_BEGIN_DECLS

#define GB_SEARCH_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_PROVIDER, GbSearchProvider))
#define GB_SEARCH_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_PROVIDER, GbSearchProvider const))
#define GB_SEARCH_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_PROVIDER, GbSearchProviderClass))
#define GB_IS_SEARCH_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_PROVIDER))
#define GB_IS_SEARCH_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_PROVIDER))
#define GB_SEARCH_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_PROVIDER, GbSearchProviderClass))

struct _GbSearchProvider
{
  GObject parent;

  /*< private >*/
  GbSearchProviderPrivate *priv;
};

struct _GbSearchProviderClass
{
  GObjectClass parent;

  gunichar     (*get_prefix)   (GbSearchProvider *provider);
  gint         (*get_priority) (GbSearchProvider *provider);
  const gchar *(*get_verb)     (GbSearchProvider *provider);
  void         (*populate)     (GbSearchProvider *provider,
                                GbSearchContext  *context,
                                const gchar      *search_terms,
                                gsize             max_results,
                                GCancellable     *cancellable);
};

gunichar     gb_search_provider_get_prefix   (GbSearchProvider *provider);
gint         gb_search_provider_get_priority (GbSearchProvider *provider);
const gchar *gb_search_provider_get_verb     (GbSearchProvider *provider);
void         gb_search_provider_populate     (GbSearchProvider *provider,
                                              GbSearchContext  *context,
                                              const gchar      *search_terms,
                                              gsize             max_results,
                                              GCancellable     *cancellable);

G_END_DECLS

#endif /* GB_SEARCH_PROVIDER_H */
