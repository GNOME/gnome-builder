/* ide-docs-provider.c
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-docs-provider"

#include "config.h"

#include <libide-threading.h>

#include "ide-docs-provider.h"

G_DEFINE_INTERFACE (IdeDocsProvider, ide_docs_provider, G_TYPE_OBJECT)

static void
ide_docs_provider_real_populate_async (IdeDocsProvider     *provider,
                                       IdeDocsItem         *item,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  ide_task_report_new_error (provider, callback, user_data,
                             ide_docs_provider_real_populate_async,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Not supported");
}

static gboolean
ide_docs_provider_real_populate_finish (IdeDocsProvider  *provider,
                                        GAsyncResult     *result,
                                        GError          **error)
{
  g_assert (IDE_IS_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_docs_provider_real_search_async (IdeDocsProvider     *provider,
                                     IdeDocsQuery        *query,
                                     IdeDocsItem         *results,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  ide_task_report_new_error (provider, callback, user_data,
                             ide_docs_provider_real_search_async,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Not supported");
}

static gboolean
ide_docs_provider_real_search_finish (IdeDocsProvider  *provider,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  g_assert (IDE_IS_DOCS_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_docs_provider_default_init (IdeDocsProviderInterface *iface)
{
  iface->populate_async = ide_docs_provider_real_populate_async;
  iface->populate_finish = ide_docs_provider_real_populate_finish;
  iface->search_async = ide_docs_provider_real_search_async;
  iface->search_finish = ide_docs_provider_real_search_finish;
}

void
ide_docs_provider_populate_async (IdeDocsProvider     *self,
                                  IdeDocsItem         *item,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DOCS_PROVIDER (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (item));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DOCS_PROVIDER_GET_IFACE (self)->populate_async (self, item, cancellable, callback, user_data);
}

gboolean
ide_docs_provider_populate_finish (IdeDocsProvider  *self,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  g_return_val_if_fail (IDE_IS_DOCS_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DOCS_PROVIDER_GET_IFACE (self)->populate_finish (self, result, error);
}

/**
 * ide_docs_provider_search_async:
 * @self: an #IdeDocsProvider
 * @query: an #IdeDocsQuery
 * @results: an #IdeDocsItem
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute
 * @user_data: closure data for @callback
 *
 * Asynchronously queries the documentation provider. The results will be placed
 * into @results. @results should contain a series of "sections" for the results
 * and then "groups" within those.
 *
 * You may not use @results outside of the main-thread.
 *
 * Since: 3.34
 */
void
ide_docs_provider_search_async (IdeDocsProvider     *self,
                                IdeDocsQuery        *query,
                                IdeDocsItem         *results,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DOCS_PROVIDER (self));
  g_return_if_fail (IDE_IS_DOCS_QUERY (query));
  g_return_if_fail (IDE_IS_DOCS_ITEM (results));

  IDE_DOCS_PROVIDER_GET_IFACE (self)->search_async (self, query, results, cancellable, callback, user_data);
}

/**
 * ide_docs_provider_search_finish:
 *
 * Since: 3.34
 */
gboolean
ide_docs_provider_search_finish (IdeDocsProvider  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (IDE_IS_DOCS_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DOCS_PROVIDER_GET_IFACE (self)->search_finish (self, result, error);
}
