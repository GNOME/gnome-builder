/* ide-search-engine.c
 *
 * Copyright 2015-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-engine"

#include "config.h"

#include <libide-plugins.h>
#include <libpeas.h>
#include <libide-core.h>
#include <libide-threading.h>

#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"
#include "ide-search-results-private.h"

#define DEFAULT_MAX_RESULTS 100

struct _IdeSearchEngine
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *extensions;
  GPtrArray              *custom_provider;
  guint                   active_count;
  GListStore             *list;
};

typedef struct
{
  IdeSearchProvider *provider;
  GListModel        *results;
  guint              truncated : 1;
} SortInfo;

typedef struct
{
  IdeTask           *task;
  char              *query;
  GArray            *sorted;
  IdeSearchCategory  category;
  guint              outstanding;
  guint              max_results;
} Request;

enum {
  PROP_0,
  PROP_BUSY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeSearchEngine, ide_search_engine, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
sort_info_clear (gpointer item)
{
  SortInfo *info = item;

  g_clear_object (&info->provider);
  g_clear_object (&info->results);
}

static Request *
request_new (void)
{
  Request *r;

  r = g_slice_new0 (Request);
  r->outstanding = 0;
  r->query = NULL;
  r->sorted = g_array_new (FALSE, FALSE, sizeof (SortInfo));
  g_array_set_clear_func (r->sorted, sort_info_clear);

  return r;
}

static void
request_destroy (Request *r)
{
  g_assert (r->outstanding == 0);
  g_clear_pointer (&r->query, g_free);
  g_clear_pointer (&r->sorted, g_array_unref);
  r->task = NULL;
  g_slice_free (Request, r);
}

static void
on_extension_added_cb (IdeExtensionSetAdapter *set,
                       PeasPluginInfo         *plugin_info,
                       GObject          *exten,
                       gpointer                user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)exten;
  IdeSearchEngine *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_ENGINE (self));

  ide_search_provider_load (provider);

  g_list_store_append (self->list, provider);
}

static void
on_extension_removed_cb (IdeExtensionSetAdapter *set,
                         PeasPluginInfo         *plugin_info,
                         GObject          *exten,
                         gpointer                user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)exten;
  IdeSearchEngine *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_ENGINE (self));

  ide_search_provider_unload (provider);

  if (self->list != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->list));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeSearchProvider) item = g_list_model_get_item (G_LIST_MODEL (self->list), i);

          if (item == provider)
            {
              g_list_store_remove (self->list, i);
              break;
            }
        }
    }
}

static void
ide_search_engine_parent_set (IdeObject *object,
                              IdeObject *parent)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;

  g_assert (IDE_IS_SEARCH_ENGINE (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  self->extensions = ide_extension_set_adapter_new (object,
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_SEARCH_PROVIDER,
                                                    "Search-Provider", NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (on_extension_added_cb),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (on_extension_removed_cb),
                    self);

  ide_extension_set_adapter_foreach (self->extensions,
                                     on_extension_added_cb,
                                     self);
}

static void
ide_search_engine_destroy (IdeObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;

  g_clear_object (&self->extensions);
  g_clear_pointer (&self->custom_provider, g_ptr_array_unref);
  g_clear_object (&self->list);

  IDE_OBJECT_CLASS (ide_search_engine_parent_class)->destroy (object);
}

static void
ide_search_engine_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeSearchEngine *self = IDE_SEARCH_ENGINE (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_search_engine_get_busy (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_engine_class_init (IdeSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_search_engine_get_property;

  i_object_class->destroy = ide_search_engine_destroy;
  i_object_class->parent_set = ide_search_engine_parent_set;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If the search engine is busy",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_search_engine_init (IdeSearchEngine *self)
{
  self->custom_provider = g_ptr_array_new_with_free_func (g_object_unref);
  self->list = g_list_store_new (IDE_TYPE_SEARCH_PROVIDER);
}

IdeSearchEngine *
ide_search_engine_new (void)
{
  return g_object_new (IDE_TYPE_SEARCH_ENGINE, NULL);
}

/**
 * ide_search_engine_get_busy:
 * @self: a #IdeSearchEngine
 *
 * Checks if the #IdeSearchEngine is currently executing a query.
 *
 * Returns: %TRUE if queries are being processed.
 */
gboolean
ide_search_engine_get_busy (IdeSearchEngine *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), FALSE);

  return self->active_count > 0;
}

static void
ide_search_engine_search_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Request *r;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);
  g_assert (r != NULL);
  g_assert (r->task == task);
  g_assert (r->outstanding > 0);

  for (guint i = 0; i < r->sorted->len; i++)
    {
      SortInfo *info = &g_array_index (r->sorted, SortInfo, i);
      gboolean truncated = FALSE;

      if (info->provider != provider)
        continue;

      g_assert (info->results == NULL);

      if (!(info->results = ide_search_provider_search_finish (provider, result, &truncated, &error)))
        {
          IDE_TRACE_MSG ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

          if (!ide_error_ignore (error))
            g_warning ("%s", error->message);
        }

      info->truncated = info->results != NULL && truncated;

#ifdef IDE_ENABLE_TRACE
      if (info->results != NULL)
        IDE_TRACE_MSG ("%s: %d results%s",
                       G_OBJECT_TYPE_NAME (provider),
                       g_list_model_get_n_items (info->results),
                       info->truncated ? " [truncated]" : "");
#endif

      break;
    }

  r->outstanding--;

  if (r->outstanding == 0)
    {
      g_autoptr(GListStore) store = g_list_store_new (G_TYPE_LIST_MODEL);
      g_autoptr(GtkFlattenListModel) flatten = gtk_flatten_list_model_new (G_LIST_MODEL (g_object_ref (store)));
      gboolean truncated = FALSE;

      for (guint i = 0; i < r->sorted->len; i++)
        {
          SortInfo *info = &g_array_index (r->sorted, SortInfo, i);

          if (info->results != NULL)
            g_list_store_append (store, info->results);

          truncated |= info->truncated;
        }

      ide_task_return_pointer (task,
                               _ide_search_results_new (G_LIST_MODEL (flatten), r->query, truncated),
                               g_object_unref);
    }
}

static void
_provider_search_async (IdeSearchProvider *provider,
                        Request           *r)
{
  SortInfo sort_info;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (r != NULL);
  g_assert (IDE_IS_TASK (r->task));

  if (r->category != IDE_SEARCH_CATEGORY_EVERYTHING &&
      r->category != ide_search_provider_get_category (provider))
    return;

  r->outstanding++;

  sort_info.provider = g_object_ref (provider);
  sort_info.results = NULL;
  g_array_append_val (r->sorted, sort_info);

  ide_search_provider_search_async (provider,
                                    r->query,
                                    r->max_results,
                                    ide_task_get_cancellable (r->task),
                                    ide_search_engine_search_cb,
                                    g_object_ref (r->task));
}

static void
ide_search_engine_search_foreach (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo   *plugin_info,
                                  GObject    *exten,
                                  gpointer          user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)exten;
  Request *r = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (r != NULL);
  g_assert (IDE_IS_TASK (r->task));

  _provider_search_async (provider, r);
}

static void
ide_search_engine_search_foreach_custom_provider (gpointer data,
                                                  gpointer user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)data;
  Request *r = user_data;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (r != NULL);
  g_assert (IDE_IS_TASK (r->task));

  _provider_search_async (provider, r);
}

void
ide_search_engine_search_async (IdeSearchEngine     *self,
                                IdeSearchCategory    category,
                                const char          *query,
                                guint                max_results,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Request *r;

  g_return_if_fail (IDE_IS_SEARCH_ENGINE (self));
  g_return_if_fail (query != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  max_results = max_results ? max_results : DEFAULT_MAX_RESULTS;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_search_engine_search_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  r = request_new ();
  r->category = category;
  r->query = g_strdup (query);
  r->max_results = max_results;
  r->task = task;
  r->outstanding = 0;
  ide_task_set_task_data (task, r, request_destroy);

  g_ptr_array_foreach (self->custom_provider,
                       ide_search_engine_search_foreach_custom_provider,
                       r);
  ide_extension_set_adapter_foreach_by_priority (self->extensions,
                                                 ide_search_engine_search_foreach,
                                                 r);

  self->active_count += r->outstanding;

  if (r->outstanding == 0)
    ide_task_return_unsupported_error (task);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

/**
 * ide_search_engine_search_finish:
 * @self: a #IdeSearchEngine
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_search_engine_search_async().
 *
 * The result is a #GListModel of #IdeSearchResult when successful. The type
 * is #IdeSearchResults which allows you to do additional filtering on the
 * result set instead of querying providers again.
 *
 * Returns: (transfer full): a #GListModel of #IdeSearchResult items.
 */
IdeSearchResults *
ide_search_engine_search_finish (IdeSearchEngine  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  IdeSearchResults *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  g_return_val_if_fail (!ret || IDE_IS_SEARCH_RESULTS (ret), NULL);

  IDE_RETURN (ret);
}

/**
 * ide_search_engine_add_provider:
 * @self: a #IdeSearchEngine
 * @provider: a #IdeSearchProvider
 *
 * Adds a custom search provider to the #IdeSearchEngine. This enables
 * to bring in custom #IdeSearchProvider during the runtime.
 */
void
ide_search_engine_add_provider (IdeSearchEngine   *self,
                                IdeSearchProvider *provider)
{
  g_return_if_fail (IDE_IS_SEARCH_ENGINE (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  g_ptr_array_add (self->custom_provider, g_object_ref (provider));
  g_list_store_append (self->list, provider);
}

/**
 * ide_search_engine_remove_provider:
 * @self: a #IdeSearchEngine
 * @provider: a #IdeSearchProvider
 *
 * Remove a custom search provider from the #IdeSearchEngine. This removes
 * custom #IdeSearchProvider during the runtime.
 */
void
ide_search_engine_remove_provider (IdeSearchEngine   *self,
                                   IdeSearchProvider *provider)
{
  g_return_if_fail (IDE_IS_SEARCH_ENGINE (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  if (self->custom_provider != NULL)
    g_ptr_array_remove (self->custom_provider, provider);

  if (self->list != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->list));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeSearchProvider) item = g_list_model_get_item (G_LIST_MODEL (self->list), i);

          if (item == provider)
            {
              g_list_store_remove (self->list, i);
              break;
            }
        }
    }
}

/**
 * ide_search_engine_list_providers:
 * @self: a #IdeSearchEngine
 *
 * Gets a #GListModel that is updated as providers are added or removed.
 *
 * Returns: (transfer full): a #GListModel of #IdeSearchProvider
 */
GListModel *
ide_search_engine_list_providers (IdeSearchEngine *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->list));
}

typedef struct
{
  const char *module_name;
  GObject *extension;
} FindByModuleName;

static void
ide_search_engine_find_by_module_name_cb (IdeExtensionSetAdapter *adapter,
                                          PeasPluginInfo         *plugin_info,
                                          GObject                *extension,
                                          gpointer                user_data)
{
  FindByModuleName *find = user_data;

  if (find->extension != NULL)
    return;

  if (ide_str_equal0 (peas_plugin_info_get_module_name (plugin_info), find->module_name))
    find->extension = extension;
}

/**
 * ide_search_engine_find_by_module_name:
 * @self: a #IdeSearchEngine
 *
 * Locates a search provider for a specific plugin module-name.
 *
 * Returns: (transfer none) (nullable): a #IdeSearchProvider or %NULL
 *
 * Since: 47
 */
IdeSearchProvider *
ide_search_engine_find_by_module_name (IdeSearchEngine *self,
                                       const char      *module_name)
{
  FindByModuleName find = {
    .module_name = module_name,
    .extension = NULL
  };

  ide_extension_set_adapter_foreach (self->extensions,
                                     ide_search_engine_find_by_module_name_cb,
                                     &find);

  return IDE_SEARCH_PROVIDER (find.extension);
}
