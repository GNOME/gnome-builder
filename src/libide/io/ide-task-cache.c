/* ide-task-cache.c
 *
 * Copyright 2015-2022 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-task-cache"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-heap.h"
#include "ide-task-cache.h"

typedef struct
{
  IdeTaskCache *self;
  gpointer      key;
  gpointer      value;
  gint64        evict_at;
} CacheItem;

typedef struct
{
  GCancellable   *cancellable;
  gpointer        key;
  GBoxedFreeFunc  key_destroy_func;
  gulong          cancelled_id;
} CancelledData;

typedef struct
{
  GSource  source;
  IdeHeap *heap;
} EvictSource;

struct _IdeTaskCache
{
  GObject               parent_instance;

  GHashFunc             key_hash_func;
  GEqualFunc            key_equal_func;
  GBoxedCopyFunc        key_copy_func;
  GBoxedFreeFunc        key_destroy_func;
  GBoxedCopyFunc        value_copy_func;
  GBoxedFreeFunc        value_destroy_func;

  IdeTaskCacheCallback  populate_callback;
  gpointer              populate_callback_data;
  GDestroyNotify        populate_callback_data_destroy;

  GHashTable           *cache;
  GHashTable           *in_flight;
  GHashTable           *queued;

  gchar                *name;

  IdeHeap              *evict_heap;
  GSource              *evict_source;
  guint                 evict_source_id;

  gint64                time_to_live_usec;
};

G_DEFINE_FINAL_TYPE (IdeTaskCache, ide_task_cache, G_TYPE_OBJECT)

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

static GParamSpec *properties [LAST_PROP];

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
      item = ide_heap_peek (ev->heap, gpointer);

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

      item = ide_heap_peek (evict_source->heap, gpointer);
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

  g_clear_pointer (&ev->heap, ide_heap_unref);
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

  g_clear_pointer (&item->key, item->self->key_destroy_func);
  g_clear_pointer (&item->value, item->self->value_destroy_func);
  item->self = NULL;
  item->evict_at = 0;

  g_slice_free (CacheItem, item);
}

static gint
cache_item_compare_evict_at (gconstpointer a,
                             gconstpointer b)
{
  const CacheItem **ci1 = (const CacheItem **)a;
  const CacheItem **ci2 = (const CacheItem **)b;
  gint64 ret;

  /*
   * While unlikely, but since we are working with 64-bit monotonic clock and
   * 32-bit return values, we can't do the normal (a - b) trick. We need to
   * ensure we are within the 32-bit boundary.
   */

  ret = (*ci2)->evict_at - (*ci1)->evict_at;

  if (ret < 0)
    return -1;
  else if (ret > 0)
    return 1;
  else
    return 0;
}

static CacheItem *
cache_item_new (IdeTaskCache  *self,
                gconstpointer  key,
                gconstpointer  value)
{
  CacheItem *ret;

  g_assert (IDE_IS_TASK_CACHE (self));

  ret = g_slice_new0 (CacheItem);
  ret->self = self;
  ret->key = self->key_copy_func ((gpointer)key);
  ret->value = self->value_copy_func ((gpointer)value);
  if (self->time_to_live_usec > 0)
    ret->evict_at = g_get_monotonic_time () + self->time_to_live_usec;

  return ret;
}

static void
cancelled_data_free (gpointer data)
{
  CancelledData *cancelled = data;

  g_clear_pointer (&cancelled->key, cancelled->key_destroy_func);

  g_cancellable_disconnect (cancelled->cancellable, cancelled->cancelled_id);
  cancelled->cancelled_id = 0;
  g_clear_object (&cancelled->cancellable);

  g_slice_free (CancelledData, cancelled);
}

static CancelledData *
cancelled_data_new (IdeTaskCache  *self,
                    GCancellable  *cancellable,
                    gconstpointer  key,
                    gulong         cancelled_id)
{
  CancelledData *ret;

  ret = g_slice_new0 (CancelledData);
  ret->cancellable = (cancellable != NULL) ? g_object_ref (cancellable) : NULL;
  ret->key = self->key_copy_func ((gpointer)key);
  ret->key_destroy_func = self->key_destroy_func;
  ret->cancelled_id = cancelled_id;

  return ret;
}

static gpointer
ide_task_cache_dummy_copy_func (gpointer boxed)
{
  return boxed;
}

static void
ide_task_cache_dummy_destroy_func (gpointer boxed)
{
}

static gboolean
ide_task_cache_evict_full (IdeTaskCache  *self,
                           gconstpointer  key,
                           gboolean       check_heap)
{
  CacheItem *item;

  g_return_val_if_fail (IDE_IS_TASK_CACHE (self), FALSE);

  if ((item = g_hash_table_lookup (self->cache, key)))
    {
      if (check_heap)
        {
          gsize i;

          for (i = 0; i < self->evict_heap->len; i++)
            {
              if (item == ide_heap_index (self->evict_heap, gpointer, i))
                {
                  ide_heap_extract_index (self->evict_heap, i, NULL);
                  break;
                }
            }
        }

      g_hash_table_remove (self->cache, key);

      g_debug ("Evicted 1 item from %s", self->name ?: "unnamed cache");

      if (self->evict_source != NULL)
        evict_source_rearm (self->evict_source);

      return TRUE;
    }

  return FALSE;
}

gboolean
ide_task_cache_evict (IdeTaskCache  *self,
                      gconstpointer  key)
{
  return ide_task_cache_evict_full (self, key, TRUE);
}

void
ide_task_cache_evict_all (IdeTaskCache *self)
{
  g_return_if_fail (IDE_IS_TASK_CACHE (self));

  while (self->evict_heap->len > 0)
    {
      CacheItem *item;

      /* The cache item is owned by the hashtable, so safe to "leak" here */
      ide_heap_extract_index (self->evict_heap, self->evict_heap->len - 1, &item);
    }

  g_hash_table_remove_all (self->cache);

  if (self->evict_source != NULL)
    evict_source_rearm (self->evict_source);
}

/**
 * ide_task_cache_peek:
 * @self: An #IdeTaskCache
 * @key: The key for the cache
 *
 * Peeks to see @key is contained in the cache and returns the
 * matching #GObject if it does.
 *
 * The reference count of the resulting #GObject is not incremented.
 * For that reason, it is important to remember that this function
 * may only be called from the main thread.
 *
 * Returns: (type GObject.Object) (nullable) (transfer none): A #GObject or
 *   %NULL if the key was not found in the cache.
 */
gpointer
ide_task_cache_peek (IdeTaskCache  *self,
                     gconstpointer  key)
{
  CacheItem *item;

  g_return_val_if_fail (IDE_IS_TASK_CACHE (self), NULL);

  if (NULL != (item = g_hash_table_lookup (self->cache, key)))
    return item->value;

  return NULL;
}

static void
ide_task_cache_propagate_error (IdeTaskCache  *self,
                                gconstpointer  key,
                                const GError  *error)
{
  GPtrArray *queued;

  g_assert (IDE_IS_TASK_CACHE (self));
  g_assert (error != NULL);

  if (NULL != (queued = g_hash_table_lookup (self->queued, key)))
    {
      /* we can't use steal because we want the key freed */
      g_ptr_array_ref (queued);
      g_hash_table_remove (self->queued, key);

      for (guint i = 0; i < queued->len; i++)
        {
          GTask *task;

          task = g_ptr_array_index (queued, i);
          g_task_return_error (task, g_error_copy (error));
        }

      g_ptr_array_unref (queued);
    }
}

static void
ide_task_cache_populate (IdeTaskCache  *self,
                         gconstpointer  key,
                         gpointer       value)
{
  CacheItem *item;

  g_assert (IDE_IS_TASK_CACHE (self));

  item = cache_item_new (self, key, value);

  if (g_hash_table_contains (self->cache, key))
    ide_task_cache_evict (self, key);
  g_hash_table_replace (self->cache, item->key, item);
  ide_heap_insert_val (self->evict_heap, item);

  if (self->evict_source != NULL)
    evict_source_rearm (self->evict_source);
}

static void
ide_task_cache_propagate_pointer (IdeTaskCache  *self,
                                  gconstpointer  key,
                                  gpointer       value)
{
  GPtrArray *queued = NULL;

  g_assert (IDE_IS_TASK_CACHE (self));

  if (NULL != (queued = g_hash_table_lookup (self->queued, key)))
    {
      g_ptr_array_ref (queued);
      g_hash_table_remove (self->queued, key);

      for (guint i = 0; i < queued->len; i++)
        {
          GTask *task = g_ptr_array_index (queued, i);

          g_task_return_pointer (task,
                                 self->value_copy_func (value),
                                 self->value_destroy_func);
        }

      g_ptr_array_unref (queued);
    }
}

static gboolean
ide_task_cache_cancel_in_idle (gpointer user_data)
{
  IdeTaskCache *self;
  CancelledData *data;
  GCancellable *cancellable;
  GPtrArray *queued;
  GTask *task = user_data;
  gboolean cancelled = FALSE;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  cancellable = g_task_get_cancellable (task);
  data = g_task_get_task_data (task);

  g_assert (IDE_IS_TASK_CACHE (self));
  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (data != NULL);
  g_assert (data->cancellable == cancellable);

  if ((queued = g_hash_table_lookup (self->queued, data->key)))
    {
      for (guint i = 0; i < queued->len; i++)
        {
          GCancellable *queued_cancellable;
          GTask *queued_task;

          queued_task = g_ptr_array_index (queued, i);
          queued_cancellable = g_task_get_cancellable (queued_task);

          if (queued_task == task && queued_cancellable == cancellable)
            {
              cancelled = g_task_return_error_if_cancelled (task);
              g_ptr_array_remove_index_fast (queued, i);
              break;
            }
        }

      if (queued->len == 0)
        {
          GTask *fetch_task;

          if ((fetch_task = g_hash_table_lookup (self->in_flight, data->key)))
            {
              GCancellable *fetch_cancellable;

              fetch_cancellable = g_task_get_cancellable (fetch_task);
              g_cancellable_cancel (fetch_cancellable);
            }
        }

      g_return_val_if_fail (cancelled, G_SOURCE_REMOVE);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_task_cache_cancelled_cb (GCancellable *cancellable,
                             gpointer      user_data)
{
  IdeTaskCache *self;
  CancelledData *data;
  GMainContext *context;
  g_autoptr(GSource) source = NULL;
  GTask *task = user_data;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  data = g_task_get_task_data (task);

  g_assert (IDE_IS_TASK_CACHE (self));
  g_assert (data != NULL);
  g_assert (data->cancellable == cancellable);

  source = g_idle_source_new ();
  g_source_set_callback (source, ide_task_cache_cancel_in_idle, g_object_ref (task), g_object_unref);
  g_source_set_name (source, "[ide] ide_task_cache_cancel_in_idle");

  context = g_main_context_get_thread_default ();
  g_source_attach (source, context);
}

static void
ide_task_cache_fetch_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeTaskCache *self = (IdeTaskCache *)object;
  GTask *task = (GTask *)result;
  GError *error = NULL;
  gpointer key = user_data;
  gpointer ret;

  g_assert (IDE_IS_TASK_CACHE (self));
  g_assert (G_IS_TASK (task));

  g_hash_table_remove (self->in_flight, key);

  ret = g_task_propagate_pointer (task, &error);

  if (ret != NULL)
    {
      ide_task_cache_populate (self, key, ret);
      ide_task_cache_propagate_pointer (self, key, ret);
      self->value_destroy_func (ret);
    }
  else
    {
      ide_task_cache_propagate_error (self, key, error);
      g_clear_error (&error);
    }

  self->key_destroy_func (key);
  g_object_unref (task);
}

void
ide_task_cache_get_async (IdeTaskCache        *self,
                          gconstpointer        key,
                          gboolean             force_update,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) fetch_task = NULL;
  g_autoptr(GTask) task = NULL;
  CancelledData *data;
  GPtrArray *queued;
  gpointer ret;
  gulong cancelled_id = 0;

  g_return_if_fail (IDE_IS_TASK_CACHE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, FALSE);

  /*
   * If we have the answer, return it now.
   */
  if (!force_update && (ret = ide_task_cache_peek (self, key)))
    {
      g_task_return_pointer (task,
                             self->value_copy_func (ret),
                             self->value_destroy_func);
      return;
    }

  /*
   * Always queue the request. If we need to dispatch the worker to
   * fetch the result, that will happen with another task.
   */
  if (!(queued = g_hash_table_lookup (self->queued, key)))
    {
      queued = g_ptr_array_new_with_free_func (g_object_unref);
      g_hash_table_replace (self->queued,
                            self->key_copy_func ((gpointer)key),
                            queued);
    }

  g_ptr_array_add (queued, g_object_ref (task));

  /*
   * The in_flight hashtable will have a bit set if we have queued
   * an operation for this key.
   */
  if (!g_hash_table_contains (self->in_flight, key))
    {
      g_autoptr(GCancellable) fetch_cancellable = NULL;

      fetch_cancellable = g_cancellable_new ();
      fetch_task = g_task_new (self,
                               fetch_cancellable,
                               ide_task_cache_fetch_cb,
                               self->key_copy_func ((gpointer)key));
      g_hash_table_replace (self->in_flight,
                            self->key_copy_func ((gpointer)key),
                            g_object_ref (fetch_task));
    }

  if (cancellable != NULL)
    {
      cancelled_id = g_cancellable_connect (cancellable,
                                            G_CALLBACK (ide_task_cache_cancelled_cb),
                                            task,
                                            NULL);
    }

  data = cancelled_data_new (self, cancellable, key, cancelled_id);
  g_task_set_task_data (task, data, cancelled_data_free);

  if (fetch_task != NULL)
    {
      self->populate_callback (self,
                               key,
                               g_object_ref (fetch_task),
                               self->populate_callback_data);
    }
}

/**
 * ide_task_cache_get_finish:
 *
 * Finish a call to ide_task_cache_get_async().
 *
 * Returns: (transfer full): The result from the cache.
 */
gpointer
ide_task_cache_get_finish (IdeTaskCache  *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_TASK_CACHE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static gboolean
ide_task_cache_do_eviction (gpointer user_data)
{
  IdeTaskCache *self = user_data;
  gint64 now = g_get_monotonic_time ();

  while (self->evict_heap->len > 0)
    {
      CacheItem *item;

      item = ide_heap_peek (self->evict_heap, gpointer);

      if (item->evict_at <= now)
        {
          ide_heap_extract (self->evict_heap, NULL);
          ide_task_cache_evict_full (self, item->key, FALSE);
          continue;
        }

      break;
    }

  return G_SOURCE_CONTINUE;
}

static void
ide_task_cache_install_evict_source (IdeTaskCache *self)
{
  GMainContext *main_context;
  EvictSource *evict_source;
  GSource *source;

  main_context = g_main_context_get_thread_default ();

  source = g_source_new (&evict_source_funcs, sizeof (EvictSource));
  g_source_set_callback (source, ide_task_cache_do_eviction, self, NULL);
  g_source_set_name (source, "IdeTaskCache Eviction");
  g_source_set_priority (source, G_PRIORITY_LOW);
  g_source_set_ready_time (source, -1);

  evict_source = (EvictSource *)source;
  evict_source->heap = ide_heap_ref (self->evict_heap);

  self->evict_source = source;
  self->evict_source_id = g_source_attach (source, main_context);
}

static void
ide_task_cache_constructed (GObject *object)
{
  IdeTaskCache *self = (IdeTaskCache *)object;

  G_OBJECT_CLASS (ide_task_cache_parent_class)->constructed (object);

  if ((self->key_equal_func == NULL) ||
      (self->key_hash_func == NULL) ||
      (self->populate_callback == NULL))
    {
      g_error ("IdeTaskCache was configured improperly.");
      return;
    }

  if (self->key_copy_func == NULL)
    self->key_copy_func = ide_task_cache_dummy_copy_func;

  if (self->key_destroy_func == NULL)
    self->key_destroy_func = ide_task_cache_dummy_destroy_func;

  if (self->value_copy_func == NULL)
    self->value_copy_func = ide_task_cache_dummy_copy_func;

  if (self->value_destroy_func == NULL)
    self->value_destroy_func = ide_task_cache_dummy_destroy_func;

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
                                           g_object_unref);

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
    ide_task_cache_install_evict_source (self);
}

static void
ide_task_cache_dispose (GObject *object)
{
  IdeTaskCache *self = (IdeTaskCache *)object;

  if (self->evict_source_id != 0)
    {
      g_source_remove (self->evict_source_id);
      self->evict_source_id = 0;
      self->evict_source = NULL;
    }

  g_clear_pointer (&self->evict_heap, ide_heap_unref);

  if (self->cache != NULL)
    {
      gint64 count;

      count = g_hash_table_size (self->cache);
      g_clear_pointer (&self->cache, g_hash_table_unref);

      g_debug ("Evicted cache of %"G_GINT64_FORMAT" items from %s",
               count, self->name ?: "unnamed cache");
    }

  if (self->queued != NULL)
    g_clear_pointer (&self->queued, g_hash_table_unref);

  if (self->in_flight != NULL)
    g_clear_pointer (&self->in_flight, g_hash_table_unref);

  if (self->populate_callback_data)
    {
      if (self->populate_callback_data_destroy)
        self->populate_callback_data_destroy (self->populate_callback_data);
    }

  G_OBJECT_CLASS (ide_task_cache_parent_class)->dispose (object);
}

static void
ide_task_cache_finalize (GObject *object)
{
  IdeTaskCache *self = (IdeTaskCache *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (ide_task_cache_parent_class)->finalize (object);
}

static void
ide_task_cache_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeTaskCache *self = IDE_TASK_CACHE(object);

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
ide_task_cache_class_init (IdeTaskCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_task_cache_constructed;
  object_class->dispose = ide_task_cache_dispose;
  object_class->finalize = ide_task_cache_finalize;
  object_class->set_property = ide_task_cache_set_property;

  properties [PROP_KEY_HASH_FUNC] =
    g_param_spec_pointer ("key-hash-func",
                         "Key Hash Func",
                         "Key Hash Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY_EQUAL_FUNC] =
    g_param_spec_pointer ("key-equal-func",
                         "Key Equal Func",
                         "Key Equal Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY_COPY_FUNC] =
    g_param_spec_pointer ("key-copy-func",
                         "Key Copy Func",
                         "Key Copy Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY_DESTROY_FUNC] =
    g_param_spec_pointer ("key-destroy-func",
                         "Key Destroy Func",
                         "Key Destroy Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_POPULATE_CALLBACK] =
    g_param_spec_pointer ("populate-callback",
                         "Populate Callback",
                         "Populate Callback",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_POPULATE_CALLBACK_DATA] =
    g_param_spec_pointer ("populate-callback-data",
                         "Populate Callback Data",
                         "Populate Callback Data",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_POPULATE_CALLBACK_DATA_DESTROY] =
    g_param_spec_pointer ("populate-callback-data-destroy",
                         "Populate Callback Data Destroy",
                         "Populate Callback Data Destroy",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTaskCache:time-to-live:
   *
   * This is the number of milliseconds before an item should be evicted
   * from the cache.
   *
   * A value of zero indicates no eviction.
   */
  properties [PROP_TIME_TO_LIVE] =
    g_param_spec_int64 ("time-to-live",
                        "Time to Live",
                        "The time to live in milliseconds.",
                        0,
                        G_MAXINT64,
                        30 * 1000,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE_COPY_FUNC] =
    g_param_spec_pointer ("value-copy-func",
                         "Value Copy Func",
                         "Value Copy Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE_DESTROY_FUNC] =
    g_param_spec_pointer ("value-destroy-func",
                         "Value Destroy Func",
                         "Value Destroy Func",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

void
ide_task_cache_init (IdeTaskCache *self)
{
  self->evict_heap = ide_heap_new (sizeof (gpointer), cache_item_compare_evict_at);
}

/**
 * ide_task_cache_new: (skip)
 */
IdeTaskCache *
ide_task_cache_new (GHashFunc            key_hash_func,
                    GEqualFunc           key_equal_func,
                    GBoxedCopyFunc       key_copy_func,
                    GBoxedFreeFunc       key_destroy_func,
                    GBoxedCopyFunc       value_copy_func,
                    GBoxedFreeFunc       value_destroy_func,
                    gint64               time_to_live,
                    IdeTaskCacheCallback populate_callback,
                    gpointer             populate_callback_data,
                    GDestroyNotify       populate_callback_data_destroy)
{
  g_return_val_if_fail (key_hash_func, NULL);
  g_return_val_if_fail (key_equal_func, NULL);
  g_return_val_if_fail (populate_callback, NULL);

  return g_object_new (IDE_TYPE_TASK_CACHE,
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
 * ide_task_cache_get_values: (skip)
 *
 * Gets all the values in the cache.
 *
 * The caller owns the resulting GPtrArray, which itself owns a reference to the children.
 *
 * Returns: (transfer container): The values.
 */
GPtrArray *
ide_task_cache_get_values (IdeTaskCache *self)
{
  GPtrArray *ar;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (IDE_IS_TASK_CACHE (self), NULL);

  ar = g_ptr_array_new_with_free_func (self->value_destroy_func);

  g_hash_table_iter_init (&iter, self->cache);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      CacheItem *item = value;

      g_ptr_array_add (ar, self->value_copy_func (item->value));
    }

  return ar;
}

void
ide_task_cache_set_name (IdeTaskCache *self,
                         const gchar  *name)
{
  g_return_if_fail (IDE_IS_TASK_CACHE (self));

  g_free (self->name);
  self->name = g_strdup (name);

  if (name && self->evict_source)
    {
      g_autofree gchar *full_name = NULL;

      full_name = g_strdup_printf ("[ide_task_cache] %s", name);
      g_source_set_name (self->evict_source, full_name);
    }
}
