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

#include "egg-counter.h"
#include "egg-task-cache.h"

struct _EggTaskCache
{
  GObject     parent_instance;

  GHashTable *cache;
  GHashTable *in_flight;
  GHashTable *queued;
};

typedef struct
{
  EggTaskCache *self;
  gchar        *key;
} CacheFault;

G_DEFINE_TYPE (EggTaskCache, egg_task_cache, G_TYPE_OBJECT)

EGG_DEFINE_COUNTER (instances,  "EggTaskCache", "Instances",  "Number of EggTaskCache instances")
EGG_DEFINE_COUNTER (in_flight,  "EggTaskCache", "In Flight",  "Number of in flight operations")
EGG_DEFINE_COUNTER (queued,     "EggTaskCache", "Queued",     "Number of queued operations")
EGG_DEFINE_COUNTER (cached,     "EggTaskCache", "Cache Size", "Number of cached items")
EGG_DEFINE_COUNTER (hits,       "EggTaskCache", "Cache Hits", "Number of cache hits")
EGG_DEFINE_COUNTER (misses,     "EggTaskCache", "Cache Size", "Number of cache misses")

static gboolean
egg_task_cache_populate_from_cache (EggTaskCache *self,
                                    const gchar  *key,
                                    GTask        *task)
{
  GObject *obj;

  g_assert (self != NULL);
  g_assert (key != NULL);
  g_assert (G_IS_TASK (task));

  obj = g_hash_table_lookup (self->cache, key);

  if (obj != NULL)
    {
      g_task_return_pointer (task, g_object_ref (obj), g_object_unref);
      return TRUE;
    }

  return FALSE;
}

static void
egg_task_cache_populate_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GTask *task = (GTask *)result;
  CacheFault *fault = user_data;
  EggTaskCache *self;
  GPtrArray *queued;
  GObject *ret;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (fault != NULL);

  self = fault->self;
  g_assert (EGG_IS_TASK_CACHE (self));

  ret = g_task_propagate_pointer (task, &error);
  g_assert (!ret || G_IS_OBJECT (ret));

  if (ret != NULL)
    {
      g_hash_table_insert (self->cache, g_strdup (fault->key), g_object_ref (ret));

      EGG_COUNTER_INC (cached);
    }

  g_hash_table_remove (self->in_flight, fault->key);

  if ((queued = g_hash_table_lookup (self->queued, fault->key)))
    {
      gsize i;

      g_hash_table_steal (self->queued, fault->key);

      for (i = 0; i < queued->len; i++)
        {
          GTask *queued_task = g_ptr_array_index (queued, i);

          if (ret != NULL)
            g_task_return_pointer (queued_task,
                                   g_object_ref (ret),
                                   g_object_unref);
          else
            g_task_return_error (queued_task, g_error_copy (error));
        }

      EGG_COUNTER_SUB (queued, queued->len);

      g_ptr_array_unref (queued);
    }

  EGG_COUNTER_DEC (in_flight);

  g_clear_object (&ret);
  g_clear_error (&error);
  g_free (fault->key);
  g_object_unref (fault->self);
  g_slice_free (CacheFault, fault);
}

void
egg_task_cache_populate (EggTaskCache    *self,
                         const gchar     *key,
                         GTask           *task,
                         GTaskThreadFunc  thread_func,
                         gpointer         task_data,
                         GDestroyNotify   task_data_destroy)
{
  GPtrArray *queued;

  g_return_if_fail (self != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (thread_func != NULL);

  if (egg_task_cache_populate_from_cache (self, key, task))
    {
      if (task_data_destroy)
        task_data_destroy (task_data);

      EGG_COUNTER_INC (hits);

      return;
    }

  EGG_COUNTER_INC (misses);

  if ((queued = g_hash_table_lookup (self->queued, key)) == NULL)
    {
      queued = g_ptr_array_new_with_free_func (g_object_unref);
      g_hash_table_insert (self->queued, g_strdup (key), queued);
    }

  g_ptr_array_add (queued, g_object_ref (task));

  EGG_COUNTER_INC (queued);

  if (!g_hash_table_contains (self->in_flight, key))
    {
      GCancellable *cancellable;
      CacheFault *fault;
      gpointer source_object;
      GTask *fault_task;

      source_object = g_task_get_source_object (task);
      cancellable = g_task_get_cancellable (task);

      fault = g_slice_new0 (CacheFault);
      fault->self = g_object_ref (self);
      fault->key = g_strdup (key);

      fault_task = g_task_new (source_object,
                               cancellable,
                               egg_task_cache_populate_cb,
                               fault);
      g_task_set_task_data (fault_task, task_data, task_data_destroy);
      g_task_run_in_thread (fault_task, thread_func);

      EGG_COUNTER_INC (in_flight);
    }
}

gboolean
egg_task_cache_evict (EggTaskCache *self,
                      const gchar  *key)
{
  g_return_val_if_fail (EGG_IS_TASK_CACHE (self), FALSE);

  if (g_hash_table_remove (self->cache, key))
    {
      EGG_COUNTER_DEC (cached);
      return TRUE;
    }

  return FALSE;
}

static void
egg_task_cache_dispose (GObject *object)
{
  EggTaskCache *self = (EggTaskCache *)object;

  if (self->cache != NULL)
    {
      EGG_COUNTER_SUB (cached, g_hash_table_size (self->cache));
      g_clear_pointer (&self->cache, g_hash_table_unref);
    }

  if (self->queued != NULL)
    {
      EGG_COUNTER_SUB (queued, g_hash_table_size (self->queued));
      g_clear_pointer (&self->queued, g_hash_table_unref);
    }

  if (self->in_flight != NULL)
    {
      EGG_COUNTER_SUB (in_flight, g_hash_table_size (self->in_flight));
      g_clear_pointer (&self->in_flight, g_hash_table_unref);
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
egg_task_cache_class_init (EggTaskCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = egg_task_cache_dispose;
  object_class->finalize = egg_task_cache_finalize;
}

void
egg_task_cache_init (EggTaskCache *self)
{
  EGG_COUNTER_INC (instances);

  self->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       g_object_unref);

  self->in_flight = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);

  self->queued = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        (GDestroyNotify)g_ptr_array_unref);
}

EggTaskCache *
egg_task_cache_new (void)
{
  return g_object_new (EGG_TYPE_TASK_CACHE, NULL);
}
