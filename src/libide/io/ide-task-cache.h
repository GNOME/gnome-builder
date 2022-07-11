/* ide-task-cache.h
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

#pragma once

#if !defined (IDE_IO_INSIDE) && !defined (IDE_IO_COMPILATION)
# error "Only <libide-io.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TASK_CACHE (ide_task_cache_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTaskCache, ide_task_cache, IDE, TASK_CACHE, GObject)

/**
 * IdeTaskCacheCallback:
 * @self: An #IdeTaskCache.
 * @key: the key to fetch
 * @task: the task to be completed
 * @user_data: user_data registered at initialization.
 *
 * #IdeTaskCacheCallback is the prototype for a function to be executed to
 * populate an item in the cache.
 *
 * This function will be executed when a fault (cache miss) occurs from
 * a caller requesting an item from the cache.
 *
 * The callee may complete the operation asynchronously, but MUST return
 * either a GObject using g_task_return_pointer() or a #GError using
 * g_task_return_error() or g_task_return_new_error().
 */
typedef void (*IdeTaskCacheCallback) (IdeTaskCache  *self,
                                      gconstpointer  key,
                                      GTask         *task,
                                      gpointer       user_data);

IDE_AVAILABLE_IN_ALL
IdeTaskCache *ide_task_cache_new        (GHashFunc              key_hash_func,
                                         GEqualFunc             key_equal_func,
                                         GBoxedCopyFunc         key_copy_func,
                                         GBoxedFreeFunc         key_destroy_func,
                                         GBoxedCopyFunc         value_copy_func,
                                         GBoxedFreeFunc         value_free_func,
                                         gint64                 time_to_live_msec,
                                         IdeTaskCacheCallback   populate_callback,
                                         gpointer               populate_callback_data,
                                         GDestroyNotify         populate_callback_data_destroy);
IDE_AVAILABLE_IN_ALL
void          ide_task_cache_set_name   (IdeTaskCache          *self,
                                         const gchar           *name);
IDE_AVAILABLE_IN_ALL
void          ide_task_cache_get_async  (IdeTaskCache          *self,
                                         gconstpointer          key,
                                         gboolean               force_update,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_cache_get_finish (IdeTaskCache          *self,
                                         GAsyncResult          *result,
                                         GError               **error);
IDE_AVAILABLE_IN_ALL
gboolean      ide_task_cache_evict      (IdeTaskCache          *self,
                                         gconstpointer          key);
IDE_AVAILABLE_IN_ALL
void          ide_task_cache_evict_all  (IdeTaskCache          *self);
IDE_AVAILABLE_IN_ALL
gpointer      ide_task_cache_peek       (IdeTaskCache          *self,
                                         gconstpointer          key);
IDE_AVAILABLE_IN_ALL
GPtrArray    *ide_task_cache_get_values (IdeTaskCache          *self);

G_END_DECLS
