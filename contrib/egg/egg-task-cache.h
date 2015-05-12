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

EggTaskCache *egg_task_cache_new      (void);
void          egg_task_cache_populate (EggTaskCache    *self,
                                       const gchar     *key,
                                       GTask           *task,
                                       GTaskThreadFunc  thread_func,
                                       gpointer         task_data,
                                       GDestroyNotify   task_data_destroy);
gboolean      egg_task_cache_evict    (EggTaskCache    *self,
                                       const gchar     *key);

G_END_DECLS

#endif /* EGG_TASK_CACHE_H */
