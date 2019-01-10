/* ide-search-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-provider"

#include "config.h"

#include <libide-threading.h>

#include "ide-search-provider.h"

G_DEFINE_INTERFACE (IdeSearchProvider, ide_search_provider, IDE_TYPE_OBJECT)

static void
ide_search_provider_real_search_async (IdeSearchProvider   *self,
                                       const gchar         *query,
                                       guint                max_results,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_SEARCH_PROVIDER (self));
  g_assert (query != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "search not implemented");
}

static GPtrArray *
ide_search_provider_real_search_finish (IdeSearchProvider  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_assert (IDE_IS_SEARCH_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_search_provider_default_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = ide_search_provider_real_search_async;
  iface->search_finish = ide_search_provider_real_search_finish;
}

void
ide_search_provider_search_async (IdeSearchProvider   *self,
                                  const gchar         *query,
                                  guint                max_results,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (self));
  g_return_if_fail (query != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SEARCH_PROVIDER_GET_IFACE (self)->search_async (self, query, max_results, cancellable, callback, user_data);
}

/**
 * ide_search_provider_search_finish:
 * @self: a #IdeSearchProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to a search provider.
 *
 * Returns: (transfer full) (element-type IdeSearchResult): a #GPtrArray
 *    of #IdeSearchResult elements.
 *
 * Since: 3.32
 */
GPtrArray *
ide_search_provider_search_finish (IdeSearchProvider  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SEARCH_PROVIDER_GET_IFACE (self)->search_finish (self, result, error);
}
