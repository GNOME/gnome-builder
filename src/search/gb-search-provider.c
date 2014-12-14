/* gb-search-provider.c
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

#include "gb-search-context.h"
#include "gb-search-provider.h"

G_DEFINE_INTERFACE (GbSearchProvider, gb_search_provider, G_TYPE_OBJECT)

static void
gb_search_provider_default_init (GbSearchProviderInterface *iface)
{
}

/**
 * gb_search_provider_get_priority:
 * @provider: A #GbSearchProvider
 *
 * Retrieves the priority of the search provider. Lower integral values are
 * of higher priority.
 *
 * Returns: An integer.
 */
gint
gb_search_provider_get_priority (GbSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_SEARCH_PROVIDER (provider), 0);

  if (GB_SEARCH_PROVIDER_GET_INTERFACE (provider)->get_priority)
    return GB_SEARCH_PROVIDER_GET_INTERFACE (provider)->get_priority (provider);
  return 0;
}

/**
 * gb_search_provider_populate:
 * @provider: A #GbSearchProvider
 * @context: A #GbSearchContext
 * @cancelalble: An optional #GCancellable to cancel the request.
 *
 * Requests that a search provider start populating @context with results.
 * If @cancellable is not %NULL, then it may be used to cancel the request.
 */
void
gb_search_provider_populate (GbSearchProvider *provider,
                             GbSearchContext  *context,
                             GCancellable     *cancellable)
{
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (GB_SEARCH_PROVIDER_GET_INTERFACE (provider)->populate)
    GB_SEARCH_PROVIDER_GET_INTERFACE (provider)->populate (provider, context,
                                                           cancellable);
}
