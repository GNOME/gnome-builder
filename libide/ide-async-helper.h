/* ide-async-helper.h
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

#ifndef IDE_ASYNC_HELPER_H
#define IDE_ASYNC_HELPER_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef void (*IdeAsyncStep) (gpointer             source_object,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data);

void ide_async_helper_run (gpointer             source_object,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data,
                           IdeAsyncStep         step1,
                           ...);

G_END_DECLS

#endif /* IDE_ASYNC_HELPER_H */
