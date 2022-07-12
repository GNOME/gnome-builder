/* ide-thread-pool.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#pragma once

#if !defined (IDE_THREADING_INSIDE) && !defined (IDE_THREADING_COMPILATION)
# error "Only <libide-threading.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libide-core.h>

G_BEGIN_DECLS

typedef struct _IdeThreadPool IdeThreadPool;

typedef enum
{
  IDE_THREAD_POOL_DEFAULT,
  IDE_THREAD_POOL_COMPILER,
  IDE_THREAD_POOL_INDEXER,
  IDE_THREAD_POOL_IO,
  IDE_THREAD_POOL_LAST
} IdeThreadPoolKind;

/**
 * IdeThreadFunc:
 * @user_data: (closure) (transfer full): The closure for the callback.
 *
 *
 */
typedef void (*IdeThreadFunc) (gpointer user_data);

IDE_AVAILABLE_IN_ALL
void ide_thread_pool_push               (IdeThreadPoolKind  kind,
                                         IdeThreadFunc      func,
                                         gpointer           func_data);
IDE_AVAILABLE_IN_ALL
void ide_thread_pool_push_with_priority (IdeThreadPoolKind  kind,
                                         gint               priority,
                                         IdeThreadFunc      func,
                                         gpointer           func_data);
IDE_AVAILABLE_IN_ALL
void ide_thread_pool_push_task          (IdeThreadPoolKind  kind,
                                         GTask             *task,
                                         GTaskThreadFunc    func);

G_END_DECLS
