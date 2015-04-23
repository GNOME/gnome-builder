/* ide-thread-pool.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_THREAD_POOL_H
#define IDE_THREAD_POOL_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum
{
  IDE_THREAD_POOL_COMPILER,
  IDE_THREAD_POOL_LAST
} IdeThreadPoolKind;

void ide_thread_pool_push_task (IdeThreadPoolKind  kind,
                                GTask             *task,
                                GTaskThreadFunc    func);

G_END_DECLS

#endif /* IDE_THREAD_POOL_H */
