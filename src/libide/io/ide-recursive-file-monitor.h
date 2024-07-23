/* ide-recursive-file-monitor.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <libdex.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_RECURSIVE_FILE_MONITOR (ide_recursive_file_monitor_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRecursiveFileMonitor, ide_recursive_file_monitor, IDE, RECURSIVE_FILE_MONITOR, GObject)

typedef DexFuture *(*IdeRecursiveIgnoreFunc) (GFile    *file,
                                              gpointer  user_data);

IDE_AVAILABLE_IN_ALL
IdeRecursiveFileMonitor *ide_recursive_file_monitor_new             (GFile                    *root);
IDE_AVAILABLE_IN_ALL
GFile                   *ide_recursive_file_monitor_get_root        (IdeRecursiveFileMonitor  *self);
IDE_AVAILABLE_IN_ALL
void                     ide_recursive_file_monitor_start_async     (IdeRecursiveFileMonitor  *self,
                                                                     GCancellable             *cancellable,
                                                                     GAsyncReadyCallback       callback,
                                                                     gpointer                  user_data);
IDE_AVAILABLE_IN_ALL
gboolean                 ide_recursive_file_monitor_start_finish    (IdeRecursiveFileMonitor  *self,
                                                                     GAsyncResult             *result,
                                                                     GError                  **error);
IDE_AVAILABLE_IN_ALL
void                     ide_recursive_file_monitor_cancel          (IdeRecursiveFileMonitor  *self);
IDE_AVAILABLE_IN_ALL
void                     ide_recursive_file_monitor_set_ignore_func (IdeRecursiveFileMonitor  *self,
                                                                     IdeRecursiveIgnoreFunc    ignore_func,
                                                                     gpointer                  ignore_func_data,
                                                                     GDestroyNotify            ignore_func_data_destroy);

G_END_DECLS
