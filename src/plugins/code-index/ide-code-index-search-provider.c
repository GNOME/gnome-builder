/* ide-code-index-search-provider.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-search-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-foundry.h>

#include "ide-code-index-search-provider.h"
#include "ide-code-index-index.h"
#include "gbp-code-index-service.h"

static void
populate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeCodeIndexIndex *index = (IdeCodeIndexIndex *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) results = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GError) error = NULL;
  gboolean truncated = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_INDEX (index));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(results = ide_code_index_index_populate_finish (index, result, &truncated, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (truncated)
    g_object_set_data (G_OBJECT (task), "TRUNCATED", GINT_TO_POINTER (truncated));

  store = g_list_store_new (IDE_TYPE_SEARCH_RESULT);
  g_list_store_splice (store, 0, 0, results->pdata, results->len);
  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static void
ide_code_index_search_provider_search_async (IdeSearchProvider   *provider,
                                             const gchar         *search_terms,
                                             guint                max_results,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  IdeCodeIndexSearchProvider *self = (IdeCodeIndexSearchProvider *)provider;
  GbpCodeIndexService *service = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeCodeIndexIndex *index;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CODE_INDEX_SEARCH_PROVIDER (self));
  g_return_if_fail (search_terms != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_code_index_search_provider_search_async);

  if (!ide_context_has_project (context) ||
      !(service = gbp_code_index_service_from_context (context)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Code index requires a project");
      IDE_EXIT;
    }

  index = gbp_code_index_service_get_index (service);

  if (index == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Code index is not currently available");
  else
    ide_code_index_index_populate_async (index,
                                         search_terms,
                                         max_results,
                                         cancellable,
                                         populate_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
ide_code_index_search_provider_search_finish (IdeSearchProvider  *provider,
                                              GAsyncResult       *result,
                                              gboolean           *truncated,
                                              GError            **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SEARCH_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  *truncated = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (result), "TRUNCATED"));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  g_return_val_if_fail (!ret || G_IS_LIST_MODEL (ret), NULL);

  IDE_RETURN (ret);
}

static char *
ide_code_index_search_provider_dup_title (IdeSearchProvider *provider)
{
  return g_strdup (_("Project Symbols"));
}

static GIcon *
ide_code_index_search_provider_dup_icon (IdeSearchProvider *provider)
{
  return g_themed_icon_new ("lang-function-symbolic");
}

static IdeSearchCategory
ide_code_index_search_provider_get_category (IdeSearchProvider *provider)
{
  return IDE_SEARCH_CATEGORY_SYMBOLS;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->search_async = ide_code_index_search_provider_search_async;
  iface->search_finish = ide_code_index_search_provider_search_finish;
  iface->dup_title = ide_code_index_search_provider_dup_title;
  iface->dup_icon = ide_code_index_search_provider_dup_icon;
  iface->get_category = ide_code_index_search_provider_get_category;
}

struct _IdeCodeIndexSearchProvider { IdeObject parent; };

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCodeIndexSearchProvider,
                         ide_code_index_search_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
ide_code_index_search_provider_init (IdeCodeIndexSearchProvider *self)
{
}

static void
ide_code_index_search_provider_class_init (IdeCodeIndexSearchProviderClass *klass)
{
}
