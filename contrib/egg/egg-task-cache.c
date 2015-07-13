/* egg-task-cache.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "egg-task-cache"

#include <glib/gi18n.h>

#include "egg-counter.h"
#include "egg-heap.h"
#include "egg-task-cache.h"

typedef struct
{
  EggTaskCache *self;
  gpointer      key;
  gpointer      value;
  gint64        evict_at;
} CacheItem;

typedef struct
{
  GSource  source;
  EggHeap *heap;
} EvictSource;

struct _EggTaskCache
{
  GObject               parent_instance;

  GHashFunc             key_hash_func;
  GEqualFunc            key_equal_func;
  GBoxedCopyFunc        key_copy_func;
  GBoxedFreeFunc        key_destroy_func;
  GBoxedCopyFunc        value_copy_func;
  GBoxedFreeFunc        value_destroy_func;

  EggTaskCacheCallback  populate_callback;
  gpointer              populate_callback_data;
  GDestroyNotify        populate_callback_data_destroy;

  GHashTable           *cache;
  GHashTable           *in_flight;
  GHashTable           *queued;

  EggHeap              *evict_heap;
  GSource              *evict_source;
  guint                 evict_source_id;

  gint64                time_to_live_usec;
};

G_DEFINE_TYPE (EggTaskCache, egg_task_cache, G_TYPE_OBJECT)

EGG_DEFINE_COUNTER (instances,  "EggTaskCache", "Instances",  "Number of EggTaskCache instances")
EGG_DEFINE_COUNTER (in_flight,  "EggTaskCache", "In Flight",  "Number of in flight operations")
EGG_DEFINE_COUNTER (queued,     "EggTaskCache", "Queued",     "Number of queued operations")
EGG_DEFINE_COUNTER (cached,     "EggTaskCache", "Cache Size", "Number of cached items")
EGG_DEFINE_COUNTER (hits,       "EggTaskCache", "Cache Hits", "Number of cache hits")
EGG_DEFINE_COUNTER (misses,     "EggTaskCache", "Cache Miss", "Number of cache misses")

enum {
  PROP_0,
  PROP_KEY_COPY_FUNC,
  PROP_KEY_DESTROY_FUNC,
  PROP_KEY_EQUAL_FUNC,
  PROP_KEY_HASH_FUNC,
  PROP_POPULATE_CALLBACK,
  PROP_POPULATE_CALLBACK_DATA,
  PROP_POPULATE_CALLBACK_DATA_DESTROY,
  PROP_TIME_TO_LIVE,
  PROP_VALUE_COPY_FUNC,
  PROP_VALUE_DESTROY_FUNC,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static gboolean
evict_source_check (GSource *source)
{
  EvictSource *ev = (EvictSource *)source;

  g_assert (ev != NULL);
  g_assert (ev->heap != NULL);

  if (ev->heap->len > 0)
    {
      CacheItem *item;
      gint64 now;

      now = g_source_get_time (source);
      item = egg_heap_peek (ev->heap, gpointer);

      return (item->evict_at <= now);
    }

  return FALSE;
}

static void
evict_source_rearm (GSource *source)
{
  EvictSource *evict_source = (EvictSource *)source;
  gint64 ready_time = -1;

  g_assert (source != NULL);
  g_assert (evict_source != NULL);

  if (evict_source->heap->len > 0)
    {
      CacheItem *item;

      item = egg_heap_peek (evict_source->heap, gpointer);
      ready_time = item->evict_at;
    }

  g_source_set_ready_time (source, ready_time);
}

static gboolean
evict_source_dispatch (GSource     *source,
                       GSourceFunc  callback,
                       gpointer     user_data)
{
  gboolean ret = G_SOURCE_CONTINUE;

  if (callback != NULL)
    ret = callback (user_data);

  evict_source_rearm (source);

  return ret;
}

static void
evict_source_finalize (GSource *source)
{
  EvictSource *ev = (EvictSource *)source;

  g_clear_pointer (&ev->heap, egg_heap_unref);
}

static GSourceFuncs evict_source_funcs = {
  NULL,
  evict_source_check,
  evict_source_dispatch,
  evict_source_finalize,
};

static void
cache_item_free (gpointer data)
{
  CacheItem *item = data;

  item->self->key_destroy_func (item->key);
  item->self->value_destroy_func (item->value);
  item->self = NULL;
  item->evict_at = 0;

  g_slice_free (CacheItem, item);
}

static gint
cache_item_compare_evict_at (gconstpointer a,
                             gconstpointer b)
{
  const CacheItem *ci1 = a;
  const CacheItem *ci2 = b;
  gint64 ret;

  /*
   * While unlikely, but since we are working with 64-bit monotonic clock and
   * 32-bit return values, we can't do the normal (a - b) trick. We need to
   * ensure we are within the 32-bit boundary.
   */

  ret = ci2->evict_at - ci1->evict_at;

  if (ret < 0)
    return -1;
  else if (ret > 0)
    return 1;
  else
    return 0;
}

static CacheItem *
cache_item_new (EggTaskCache  *self,
                gconstpointer  key,
                gconstpointer  value)
{
  CacheItem *ret;

  g_assert (EGG_IS_TASK_CACHE (self));

  ret = g_slice_new0 (CacheItem);
  ret->self = self;
  ret->key = self->key_copy_func ((gpointer)key);
  ret->value = self->value_copy_func ((gpointer)value);
  if (self->time_to_live_usec > 0)
    ret->evict_at = g_get_monotonic_time () + self->time_to_live_usec;

  return ret;
}

static gboolean
egg_task_cache_evict_full (EggTaskCache  *self,
                           gconstpointer  key,
                           gboolean       check_heap)
{
  CacheItem *item;

  g_return_val_if_fail (EGG_IS_TASK_CACHE (self), FALSE);

  if ((item = g_hash_table_lookup (self->cache, key)))
    {
      if (check_heap)
        {
          gsize i;

          for (i = 0; i < self->evict_heap->len; i++)
            {
              if (item == egg_heap_index (self->evict_heap, gpointer, i))
                {
                  egg_heap_extract_index (self->evict_heap, i, NULL);
                  break;
                }
            }
        }

      g_hash_table_remove (self->cache, key);

      EGG_COUNTER_DEC (cached);

      if (self->evict_source != NULL)
        evict_source_rearm (self->evict_source);

      return TRUE;
    }

  return FALSE;
}

gboolean
egg_task_cache_evict (EggTaskCache  *self,
                      gconstpointer  key)
{
  return egg_task_cache_evict_full (self, key, TRUE);
}

/**
 * egg_task_cache_peek:
 * @self: An #EggTaskCache
 * @key: The key for the cache
 *
 * Peeks to see @key is contained in the cache and returns the
 * matching #GObject if it does.
 *
 * The reference count of the resulting #GObject is not incremented.
 * For that reason, it is important to remember that this function
 * may only be called from the main thread.
 *
 * Returns: (type GObject) (nullable): A #GObject or %NULL if the
 *   key was not found in the cache.
 */
gpointer
egg_task_cache_peek (EggTaskCache  *self,
                     gconstpointer  key)
{
  CacheItem *item;

  g_return_val_if_fail (EGG_IS_TASK_CACHE (self), NULL);

  if ((item = g_hash_table_lookup (self->cache, key)))
    {
      EGG_COUNTER_INC (hits);
      return item->value;
    }

  return NULL;
}

static void
egg_task_cache_propagate_error (EggTaskCache  *self,
                                gconstpointer  key,
                                const GError  *error)
{
  GPtrArray *queued;

  g_assert (EGG_IS_TASK_CACHE (self));
  g_assert (error != NULL);

  if ((queued = g_hash_table_lookup (self->queued, key)))
    {
      gint64 count = queued->len;
      gsize i;

      /* we can't use steal because we want the key freed */
      g_ptr_array_ref (queued);
      g_hash_table_remove (self->queued, key);

      for (i = 0; i < queued->len; i++)
        {
          GTask *task;

          task = g_ptr_array_index (queued, i);
          g_task_return_error (task, g_error_copy (error));
        }

      g_ptr_array_unref (queued);

      EGG_COUNTER_SUB (queued, count);
    }
}

static void
egg_task_cache_populate (EggTaskCache  *self,
                         gconstpointer  key,
                         gpointer       value)
{
  CacheItem *item;

  g_assert (EGG_IS_TASK_CACHE (self));

  item = cache_item_new (self, key, value);

  if (g_hash_table_contains (self->cache, key))
    egg_task_cache_evict (self, key);
  g_hash_table_insert (self->cache, item->key, item);
  egg_heap_insert_val (self->evict_heap, item);

  EGG_COUNTER_INC (cached);

  if (self->evict_source != NULL)
    evict_source_rearm (self->evict_source);
}

static void
egg_task_cache_propagate_pointer (EggTaskCache  *self,
                                  gconstpointer  key,
                                  gpointer       value)
{
  GPtrArray *queued = NULL;

  g_assert (EGG_IS_TASK_CACHE (self));

  if ((queued = g_hash_table_lookup (self->queued, key)))
    {
      gint64 count = queued->len;
      gsize i;

      g_ptr_array_ref (queued);
      g_hash_table_remove (self->queued, key);

      for (i = 0; i < queued->len; i++)
        {
          GTask *task;

          task = g_ptr_array_index (queued, i);
          g_task_return_pointer (task,
                                 self->value_copy_func (value),
                                 self->value_destroy_func);
        }

      g_ptr_array_unref (queued);

      EGG_COUNTER_SUB (queued, count);
    }
}

static void
egg_task_cache_fetch_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  EggTaskCache *self = (EggTaskCache *)object;
  GTask *task = (GTask *)result;
  GError *error = NULL;
  gpointer key = user_data;
  gpointer ret;

  g_assert (EGG_IS_TASK_CACHE (self));
  g_assert (G_IS_TASK (task));

  g_hash_table_remove (self->in_flight, key);

  ret = g_task_propagate_pointer (task, &error);

  if (ret != NULL)
    {
      egg_task_cache_populate (self, key, ret);
      egg_task_cache_propagate_pointer (self, key, ret);
      self->value_destroy_func (ret);
    }
  else
    {
      egg_task_cache_propagate_error (self, key, error);
      g_clear_error (&error);
    }

  self->key_destroy_func (key);

  EGG_COUNTER_DEC (in_flight);
}

void
egg_task_cache_get_async (EggTaskCache        *self,
                          gconstpointer        key,
                          gboolean             force_update,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GPtrArray *queued;
  gpointer ret;

  g_return_if_fail (EGG_IS_TASK_CACHE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /*
   * If we have the answer, return it now.
   */
  if (!force_update && (ret = egg_task_cache_peek (self, key)))
    {
      g_task_return_pointer (task,
                             self->value_copy_func (ret),
                             self->value_destroy_func);
      return;
    }

  EGG_COUNTER_INC (misses);

  /*
   * Always queue the request. If we need to dispatch the worker to
   * fetch the result, that will happen with another task.
   */
  if (!(queued = g_hash_table_lookup (self->queued, key)))
    {
      queued = g_ptr_array_new_with_free_func (g_object_unref);
      g_hash_table_insert (self->queued,
                           self->key_copy_func ((gpointer)key),
                           queued);
    }

  g_ptr_array_add (queued, g_object_ref (task));
  EGG_COUNTER_INC (queued);

  /*
   * The in_flight hashtable will have a bit set if we have queued
   * an operation for this key.
   */
  if (!g_hash_table_contains (self->in_flight, key))
    {
      g_autoptr(GTask) fetch_task = NULL;

      fetch_task = g_task_new (self,
                               cancellable,
                               egg_task_cache_fetch_cb,
                               self->key_copy_func ((gpointer)key));
      g_hash_table_insert (self->in_flight,
                           self->key_copy_func ((gpointer)key),
                           GINT_TO_POINTER (TRUE));
      self->populate_callback (self,
                               key,
                               g_object_ref (fetch_task),
                               self->populate_callback_data);

      EGG_COUNTER_INC (in_flight);
    }
}

gpointer
egg_task_cache_get_finish (EggTaskCache  *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (EGG_IS_TASK_CACHE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static gboolean
egg_task_cache_do_eviction (gpointer user_data)
{
  EggTaskCache *self = user_data;
  gint64 now = g_get_monotonic_time ();

  while (self->evict_heap->len > 0)
    {
      CacheItem *item;

      item = egg_heap_peek (self->evict_heap, gpointer);

      if (item->evict_at <= now)
        {
          egg_heap_extract (self->evict_heap, NULL);
          egg_task_cache_evict_full (self, item->key, FALSE);
          continue;
        }

      break;
    }

  return G_SOURCE_CONTINUE;
}

static void
egg_task_cache_install_evict_source (EggTaskCache *self)
{
  GMainContext *main_context;
  EvictSource *evict_source;
  GSource *source;

  main_context = g_main_context_get_thread_default ();

  source = g_source_new (&evict_source_funcs, sizeof (EvictSource));
  g_source_set_callback (source, egg_task_cache_do_eviction, self, NULL);
  g_source_set_name (source, "EggTaskCache Eviction");
  g_source_set_priority (source, G_PRIORITY_LOW);
  g_source_set_ready_time (source, -1);

  evict_source = (EvictSource *)source;
  evict_source->heap = egg_heap_ref (self->evict_heap);

  self->evict_source = source;
  self->evict_source_id = g_source_attach (source, main_context);
}

static void
egg_task_cache_constructed (GObject *object)
{
  EggTaskCache *self = (EggTaskCache *)object;

  G_OBJECT_CLASS (egg_task_cache_parent_class)->constructed (object);

  if ((self->key_copy_func == NULL) ||
      (self->key_destroy_func == NULL) ||
      (self->key_equal_func == NULL) ||
      (self->key_hash_func == NULL) ||
      (self->value_copy_func == NULL) ||
      (self->value_destroy_func == NULL) ||
      (self->populate_callback == NULL))
    {
      g_error ("EggTaskCache was configured improperly.");
      return;
    }

  /*
   * This is where the cached result objects live.
   */
  self->cache = g_hash_table_new_full (self->key_hash_func,
                                       self->key_equal_func,
                                       NULL,
                                       cache_item_free);

  /*
   * This is where we store a bit to know if we have an inflight
   * request for this cache key.
   */
  self->in_flight = g_hash_table_new_full (self->key_hash_func,
                                           self->key_equal_func,
                                           self->key_destroy_func,
                                           NULL);

  /*
   * This is where tasks queue waiting for an in_flight callback.
   */
  self->queued = g_hash_table_new_full (self->key_hash_func,
                                        self->key_equal_func,
                                        self->key_destroy_func,
                                        (GDestroyNotify)g_ptr_array_unref);

  /*
   * Register our eviction source if we have a time_to_live.
   */
  if (self->time_to_live_usec > 0)
    egg_task_cache_install_evict_source (self);
}

static void
count_queued_cb (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GPtrArray *ar = value;
  gint64 *count = user_data;

  (*count) += ar->len;
}

static void
egg_task_cache_dispose (GObject *object)
{
  EggTaskCache *self = (EggTaskCache *)object;

  if (self->evict_source_id != 0)
    {
      g_source_remove (self->evict_source_id);
      self->evict_source_id = 0;
      self->evict_source = NULL;
    }

  g_clear_pointer (&self->evict_heap, egg_heap_unref);

  if (self->cache != NULL)
    {
      gint64 count;

      count = g_hash_table_size (self->cache);
      g_clear_pointer (&self->cache, g_hash_table_unref);

      EGG_COUNTER_SUB (cached, count);
    }

  if (self->queued != NULL)
    {
      gint64 count = 0;

      g_hash_table_foreach (self->queued, count_queued_cb, &count);
      g_clear_pointer (&self->queued, g_hash_table_unref);

      EGG_COUNTER_SUB (queued, count);
    }

  if (self->in_flight != NULL)
    {
      gint64 count;

      count = g_hash_table_size (self->in_flight);
      g_clear_pointer (&self->in_flight, g_hash_table_unref);

      EGG_COUNTER_SUB (in_flight, count);
    }

  if (self->populate_callback_data)
    {
      if (self->populate_callback_data_destroy)
        self->populate_callback_data_destroy (self->populate_callback_data);
    }

  G_OBJECT_CLASS (egg_task_cache_parent_class)->dispose (object);
}

static void
egg_task_cache_finalize (GObject *object)
{
  G_OBJECT_CLASS (egg_task_cache_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}

static void
egg_task_cache_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EggTaskCache *self = EGG_TASK_CACHE(object);

  switch (prop_id)
    {
    case PROP_KEY_COPY_FUNC:
      self->key_copy_func = g_value_get_pointer (value);
      break;

    case PROP_KEY_DESTROY_FUNC:
      self->key_destroy_func = g_value_get_pointer (value);
      break;

    case PROP_KEY_EQUAL_FUNC:
      self->key_equal_func = g_value_get_pointer (value);
      break;

    case PROP_KEY_HASH_FUNC:
      self->key_hash_func = g_value_get_pointer (value);
      break;

    case PROP_POPULATE_CALLBACK:
      self->populate_callback = g_value_get_pointer (value);
      break;

    case PROP_POPULATE_CALLBACK_DATA:
      self->populate_callback_data = g_value_get_pointer (value);
      break;

    case PROP_POPULATE_CALLBACK_DATA_DESTROY:
      self->populate_callback_data_destroy = g_value_get_pointer (value);
      break;

    case PROP_TIME_TO_LIVE:
      self->time_to_live_usec = (g_value_get_int64 (value) * 1000L);
      break;

    case PROP_VALUE_COPY_FUNC:
      self->value_copy_func = g_value_get_pointer (value);
      break;

    case PROP_VALUE_DESTROY_FUNC:
      self->value_destroy_func = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
egg_task_cache_class_init (EggTaskCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = egg_task_cache_constructed;
  object_class->dispose = egg_task_cache_dispose;
  object_class->finalize = egg_task_cache_finalize;
  object_class->set_property = egg_task_cache_set_property;

  gParamSpecs [PROP_KEY_HASH_FUNC] =
    g_param_spec_pointer ("key-hash-func",
                         _("Key Hash Func"),
                         _("Key Hash Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_KEY_EQUAL_FUNC] =
    g_param_spec_pointer ("key-equal-func",
                         _("Key Equal Func"),
                         _("Key Equal Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_KEY_COPY_FUNC] =
    g_param_spec_pointer ("key-copy-func",
                         _("Key Copy Func"),
                         _("Key Copy Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_KEY_DESTROY_FUNC] =
    g_param_spec_pointer ("key-destroy-func",
                         _("Key Destroy Func"),
                         _("Key Destroy Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_POPULATE_CALLBACK] =
    g_param_spec_pointer ("populate-callback",
                         _("Populate Callback"),
                         _("Populate Callback"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_POPULATE_CALLBACK_DATA] =
    g_param_spec_pointer ("populate-callback-data",
                         _("Populate Callback Data"),
                         _("Populate Callback Data"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_POPULATE_CALLBACK_DATA_DESTROY] =
    g_param_spec_pointer ("populate-callback-data-destroy",
                         _("Populate Callback Data Destroy"),
                         _("Populate Callback Data Destroy"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EggTaskCache:time-to-live:
   *
   * This is the number of milliseconds before an item should be evicted
   * from the cache.
   *
   * A value of zero indicates no eviction.
   */
  gParamSpecs [PROP_TIME_TO_LIVE] =
    g_param_spec_int64 ("time-to-live",
                        _("Time to Live"),
                        _("The time to live in milliseconds."),
                        0,
                        G_MAXINT64,
                        30 * 1000,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_VALUE_COPY_FUNC] =
    g_param_spec_pointer ("value-copy-func",
                         _("Value Copy Func"),
                         _("Value Copy Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_VALUE_DESTROY_FUNC] =
    g_param_spec_pointer ("value-destroy-func",
                         _("Value Destroy Func"),
                         _("Value Destroy Func"),
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

void
egg_task_cache_init (EggTaskCache *self)
{
  EGG_COUNTER_INC (instances);

  self->evict_heap = egg_heap_new (sizeof (gpointer),
                                   cache_item_compare_evict_at);
}

EggTaskCache *
egg_task_cache_new (GHashFunc            key_hash_func,
                    GEqualFunc           key_equal_func,
                    GBoxedCopyFunc       key_copy_func,
                    GBoxedFreeFunc       key_destroy_func,
                    GBoxedCopyFunc       value_copy_func,
                    GBoxedFreeFunc       value_destroy_func,
                    gint64               time_to_live,
                    EggTaskCacheCallback populate_callback,
                    gpointer             populate_callback_data,
                    GDestroyNotify       populate_callback_data_destroy)
{
  g_return_val_if_fail (key_hash_func, NULL);
  g_return_val_if_fail (key_equal_func, NULL);
  g_return_val_if_fail (key_copy_func, NULL);
  g_return_val_if_fail (key_destroy_func, NULL);
  g_return_val_if_fail (populate_callback, NULL);

  return g_object_new (EGG_TYPE_TASK_CACHE,
                       "key-hash-func", key_hash_func,
                       "key-equal-func", key_equal_func,
                       "key-copy-func", key_copy_func,
                       "key-destroy-func", key_destroy_func,
                       "populate-callback", populate_callback,
                       "populate-callback-data", populate_callback_data,
                       "populate-callback-data-destroy", populate_callback_data_destroy,
                       "time-to-live", time_to_live,
                       "value-copy-func", value_copy_func,
                       "value-destroy-func", value_destroy_func,
                       NULL);
}

/**
 * egg_task_cache_get_values:
 *
 * Gets all the values in the cache.
 *
 * The caller owns the resulting GPtrArray, which itself owns a reference to the children.
 *
 * Returns: (transfer container) (element-type gpointer): The values.
 */
GPtrArray *
egg_task_cache_get_values (EggTaskCache *self)
{
  GPtrArray *ar;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (EGG_IS_TASK_CACHE (self), NULL);

  ar = g_ptr_array_new_with_free_func (self->value_destroy_func);

  g_hash_table_iter_init (&iter, self->cache);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      CacheItem *item = value;

      g_ptr_array_add (ar, self->value_copy_func (item->value));
    }

  return ar;
}
