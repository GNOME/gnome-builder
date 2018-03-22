/* ide-search-engine.c
 *
 * Copyright © 2015-2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-engine"

#include "config.h"

#include <libpeas/peas.h>

#include "search/ide-search-engine.h"
#include "search/ide-search-provider.h"
#include "search/ide-search-result.h"
#include "threading/ide-task.h"

#define DEFAULT_MAX_RESULTS 50

struct _IdeSearchEngine
{
  IdeObject         parent_instance;
  PeasExtensionSet *extensions;
  guint             active_count;
};

typedef struct
{
  IdeTask       *task;
  gchar         *query;
  GListStore    *store;
  guint          outstanding;
  guint          max_results;
} Request;

enum {
  PROP_0,
  PROP_BUSY,
  N_PROPS
};

G_DEFINE_TYPE (IdeSearchEngine, ide_search_engine, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static Request *
request_new (void)
{
  Request *r;

  r = g_slice_new0 (Request);
  r->store = NULL;
  r->outstanding = 0;
  r->query = NULL;

  return r;
}

static void
request_destroy (Request *r)
{
  g_assert (r->outstanding == 0);
  g_clear_object (&r->store);
  g_clear_pointer (&r->query, g_free);
  r->task = NULL;
  g_slice_free (Request, r);
}

static void
ide_search_engine_constructed (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;
  IdeContext *context;

  g_assert (IDE_IS_SEARCH_ENGINE (self));

  G_OBJECT_CLASS (ide_search_engine_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_SEARCH_PROVIDER,
                                             "context", context,
                                             NULL);
}

static void
ide_search_engine_dispose (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;

  g_clear_object (&self->extensions);

  G_OBJECT_CLASS (ide_search_engine_parent_class)->dispose (object);
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

  object_class->constructed = ide_search_engine_constructed;
  object_class->dispose = ide_search_engine_dispose;
  object_class->get_property = ide_search_engine_get_property;

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
  g_autoptr(GPtrArray) ar = NULL;
  Request *r;

  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);
  g_assert (r != NULL);
  g_assert (r->task == task);
  g_assert (r->outstanding > 0);
  g_assert (G_IS_LIST_STORE (r->store));

  ar = ide_search_provider_search_finish (provider, result, &error);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      goto cleanup;
    }

  for (guint i = 0; i < ar->len; i++)
    {
      IdeSearchResult *item = g_ptr_array_index (ar, i);

      g_assert (IDE_IS_SEARCH_RESULT (item));

      g_list_store_insert_sorted (r->store,
                                  item,
                                  (GCompareDataFunc)ide_search_result_compare,
                                  NULL);

    }

cleanup:
  r->outstanding--;

  if (r->outstanding == 0)
    ide_task_return_pointer (task, g_steal_pointer (&r->store), g_object_unref);
}

static void
ide_search_engine_search_foreach (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  PeasExtension    *exten,
                                  gpointer          user_data)
{
  IdeSearchProvider *provider = (IdeSearchProvider *)exten;
  Request *r = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (r != NULL);
  g_assert (IDE_IS_TASK (r->task));
  g_assert (G_IS_LIST_STORE (r->store));

  r->outstanding++;

  ide_search_provider_search_async (provider,
                                    r->query,
                                    r->max_results,
                                    ide_task_get_cancellable (r->task),
                                    ide_search_engine_search_cb,
                                    g_object_ref (r->task));
}

void
ide_search_engine_search_async (IdeSearchEngine     *self,
                                const gchar         *query,
                                guint                max_results,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GListStore) store = NULL;
  Request *r;

  g_return_if_fail (IDE_IS_SEARCH_ENGINE (self));
  g_return_if_fail (query != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  max_results = max_results ? max_results : DEFAULT_MAX_RESULTS;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_search_engine_search_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  r = request_new ();
  r->query = g_strdup (query);
  r->max_results = max_results;
  r->task = task;
  r->store = g_list_store_new (IDE_TYPE_SEARCH_RESULT);
  r->outstanding = 0;
  ide_task_set_task_data (task, r, (GDestroyNotify)request_destroy);

  peas_extension_set_foreach (self->extensions,
                              ide_search_engine_search_foreach,
                              r);

  self->active_count += r->outstanding;

  if (r->outstanding == 0)
    ide_task_return_pointer (task,
                             g_object_ref (r->store),
                             g_object_unref);

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
 * The result is a #GListModel of #IdeSearchResult when successful.
 *
 * Returns: (transfer full): a #GListModel of #IdeSearchResult items.
 */
GListModel *
ide_search_engine_search_finish (IdeSearchEngine  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
