/*
 * gbp-manuals-search-provider.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libdex.h>

#include "gbp-manuals-application-addin.h"
#include "gbp-manuals-search-provider.h"
#include "gbp-manuals-search-result.h"

#include "manuals-repository.h"
#include "manuals-search-query.h"

struct _GbpManualsSearchProvider
{
  IdeObject parent_instance;
  GbpManualsApplicationAddin *app_addin;
};

static void
gbp_manuals_search_provider_load (IdeSearchProvider *provider)
{
  GbpManualsSearchProvider *self = GBP_MANUALS_SEARCH_PROVIDER (provider);

  self->app_addin = ide_application_find_addin_by_module_name (IDE_APPLICATION_DEFAULT, "manuals");
}

static void
gbp_manuals_search_provider_unload (IdeSearchProvider *provider)
{
  GbpManualsSearchProvider *self = GBP_MANUALS_SEARCH_PROVIDER (provider);

  self->app_addin = NULL;
}

typedef struct
{
  ManualsSearchQuery *query;
  DexFuture *repository;
} Search;

static void
search_free (Search *search)
{
  g_clear_object (&search->query);
  dex_clear (&search->repository);
  g_free (search);
}

static gpointer
gbp_manuals_search_provider_map_func (gpointer item,
                                      gpointer user_data)
{
  return gbp_manuals_search_result_new (g_steal_pointer (&item));
}

static DexFuture *
gbp_manuals_search_provider_search_fiber (gpointer user_data)
{
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GtkMapListModel) map_model = NULL;
  g_autoptr(GError) error = NULL;
  Search *search = user_data;

  if (!(repository = dex_await_object (dex_ref (search->repository), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (manuals_search_query_execute (search->query, repository), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  map_model = gtk_map_list_model_new (g_object_ref (G_LIST_MODEL (search->query)),
                                      gbp_manuals_search_provider_map_func,
                                      NULL, NULL);

  return dex_future_new_take_object (g_object_ref (G_LIST_MODEL (map_model)));
}

static void
gbp_manuals_search_provider_search_async (IdeSearchProvider   *provider,
                                          const gchar         *query,
                                          guint                max_results,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpManualsSearchProvider *self = GBP_MANUALS_SEARCH_PROVIDER (provider);
  g_autoptr(DexAsyncResult) result = NULL;
  g_autoptr(ManualsSearchQuery) query_obj = NULL;
  g_autofree char *query_str = NULL;
  Search *search;

  g_assert (GBP_IS_MANUALS_SEARCH_PROVIDER (self));
  g_assert (query != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  query_str = g_strstrip (g_strdup (query));
  query_obj = manuals_search_query_new ();
  manuals_search_query_set_text (query_obj, query_str);

  result = dex_async_result_new (self, cancellable, callback, user_data);

  if (strlen (query_str) < 3)
    {
      dex_async_result_await (result,
                              dex_future_new_take_object (g_list_store_new (IDE_TYPE_SEARCH_RESULT)));
      return;
    }

  search = g_new0 (Search, 1);
  search->query = g_steal_pointer (&query_obj);
  search->repository = gbp_manuals_application_addin_load_repository (self->app_addin);

  dex_async_result_await (result,
                          dex_scheduler_spawn (NULL, 0,
                                               gbp_manuals_search_provider_search_fiber,
                                               search,
                                               (GDestroyNotify)search_free));
}

static GListModel *
gbp_manuals_search_provider_search_finish (IdeSearchProvider  *self,
                                           GAsyncResult       *result,
                                           gboolean           *truncated,
                                           GError            **error)
{
  *truncated = FALSE;

  return dex_async_result_propagate_pointer (DEX_ASYNC_RESULT (result), error);
}

static IdeSearchCategory
gbp_manuals_search_provider_get_category (IdeSearchProvider *provider)
{
  return IDE_SEARCH_CATEGORY_DOCUMENTATION;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = gbp_manuals_search_provider_load;
  iface->unload = gbp_manuals_search_provider_unload;
  iface->search_async = gbp_manuals_search_provider_search_async;
  iface->search_finish = gbp_manuals_search_provider_search_finish;
  iface->get_category = gbp_manuals_search_provider_get_category;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsSearchProvider, gbp_manuals_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_manuals_search_provider_class_init (GbpManualsSearchProviderClass *klass)
{
}

static void
gbp_manuals_search_provider_init (GbpManualsSearchProvider *self)
{
}
