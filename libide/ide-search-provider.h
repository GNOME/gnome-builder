/* ide-search-provider.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_SEARCH_PROVIDER_H
#define IDE_SEARCH_PROVIDER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_PROVIDER (ide_search_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSearchProvider, ide_search_provider,
                          IDE, SEARCH_PROVIDER, IdeObject)

struct _IdeSearchProviderClass
{
  IdeObjectClass parent_class;

  gunichar     (*get_prefix)   (IdeSearchProvider *provider);
  gint         (*get_priority) (IdeSearchProvider *provider);
  const gchar *(*get_verb)     (IdeSearchProvider *provider);
  void         (*populate)     (IdeSearchProvider *provider,
                                IdeSearchContext  *context,
                                const gchar       *search_terms,
                                gsize              max_results,
                                GCancellable      *cancellable);
};

gunichar     ide_search_provider_get_prefix   (IdeSearchProvider *provider);
gint         ide_search_provider_get_priority (IdeSearchProvider *provider);
const gchar *ide_search_provider_get_verb     (IdeSearchProvider *provider);
void         ide_search_provider_populate     (IdeSearchProvider *provider,
                                               IdeSearchContext  *context,
                                               const gchar       *search_terms,
                                               gsize              max_results,
                                               GCancellable      *cancellable);

G_END_DECLS

#endif /* IDE_SEARCH_PROVIDER_H */
