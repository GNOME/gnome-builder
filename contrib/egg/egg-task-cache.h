/* egg-task-cache.h
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

#ifndef EGG_TASK_CACHE_H
#define EGG_TASK_CACHE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EGG_TYPE_TASK_CACHE (egg_task_cache_get_type())

G_DECLARE_FINAL_TYPE (EggTaskCache, egg_task_cache, EGG, TASK_CACHE, GObject)

/**
 * EggTaskCacheCallback:
 * @self: An #EggTaskCache.
 * @key: the key to fetch
 * @task: the task to be completed
 * @user_data: user_data registered at initialization.
 *
 * #EggTaskCacheCallback is the prototype for a function to be executed to
 * populate a an item in the cache.
 *
 * This function will be executed when a fault (cache miss) occurs from
 * a caller requesting an item from the cache.
 *
 * The callee may complete the operation asynchronously, but MUST return
 * either a GObject using g_task_return_pointer() or a #GError using
 * g_task_return_error() or g_task_return_new_error().
 */
typedef void (*EggTaskCacheCallback) (EggTaskCache  *self,
                                      gconstpointer  key,
                                      GTask         *task,
                                      gpointer       user_data);

EggTaskCache *egg_task_cache_new        (GHashFunc              key_hash_func,
                                         GEqualFunc             key_equal_func,
                                         GBoxedCopyFunc         key_copy_func,
                                         GBoxedFreeFunc         key_destroy_func,
                                         GBoxedCopyFunc         value_copy_func,
                                         GBoxedFreeFunc         value_free_func,
                                         gint64                 time_to_live_msec,
                                         EggTaskCacheCallback   populate_callback,
                                         gpointer               populate_callback_data,
                                         GDestroyNotify         populate_callback_data_destroy);
void          egg_task_cache_get_async  (EggTaskCache          *self,
                                         gconstpointer          key,
                                         gboolean               force_update,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data);
gpointer      egg_task_cache_get_finish (EggTaskCache          *self,
                                         GAsyncResult          *result,
                                         GError               **error);
gboolean      egg_task_cache_evict      (EggTaskCache          *self,
                                         gconstpointer          key);
gpointer      egg_task_cache_peek       (EggTaskCache          *self,
                                         gconstpointer          key);
GPtrArray    *egg_task_cache_get_values (EggTaskCache          *self);

G_END_DECLS

#endif /* EGG_TASK_CACHE_H */
