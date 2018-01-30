/* ide-glib.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
const gchar *ide_gettext                              (const gchar          *message);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_boolean_from_main      (GTask                *task,
                                                       gboolean              value);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_int_from_main          (GTask                *task,
                                                       gint                  value);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_pointer_from_main      (GTask                *task,
                                                       gpointer              value,
                                                       GDestroyNotify        notify);
IDE_AVAILABLE_IN_ALL
void         ide_g_task_return_error_from_main        (GTask                *task,
                                                       GError               *error);
IDE_AVAILABLE_IN_3_28
gchar       *ide_g_file_get_uncanonical_relative_path (GFile                *file,
                                                       GFile                *other);
IDE_AVAILABLE_IN_3_28
void         ide_g_file_find_async                    (GFile                *file,
                                                       const gchar          *pattern,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_28
GPtrArray   *ide_g_file_find_finish                   (GFile                *file,
                                                       GAsyncResult         *result,
                                                       GError              **error);
IDE_AVAILABLE_IN_3_28
void         ide_g_file_get_children_async            (GFile                *file,
                                                       const gchar          *attributes,
                                                       GFileQueryInfoFlags   flags,
                                                       gint                  io_priority,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_3_28
GPtrArray   *ide_g_file_get_children_finish           (GFile                *file,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS
