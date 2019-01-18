/* ide-gfile.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_IO_INSIDE) && !defined (IDE_IO_COMPILATION)
# error "Only <libide-io.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

IDE_AVAILABLE_IN_3_32
gboolean   ide_path_is_ignored                      (const gchar          *path);
IDE_AVAILABLE_IN_3_32
gboolean   ide_g_file_is_ignored                    (GFile                *file);
IDE_AVAILABLE_IN_3_32
void       ide_g_file_add_ignored_pattern           (const gchar          *pattern);
IDE_AVAILABLE_IN_3_32
gchar     *ide_g_file_get_uncanonical_relative_path (GFile                *file,
                                                     GFile                *other);
IDE_AVAILABLE_IN_3_32
GPtrArray *ide_g_file_find_with_depth               (GFile                *file,
                                                     const gchar          *pattern,
                                                     guint                 max_depth,
                                                     GCancellable         *cancellable);
IDE_AVAILABLE_IN_3_32
void       ide_g_file_find_with_depth_async         (GFile                *file,
                                                     const gchar          *pattern,
                                                     guint                 max_depth,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_3_32
void       ide_g_file_find_async                    (GFile                *file,
                                                     const gchar          *pattern,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_3_32
GPtrArray *ide_g_file_find_finish                   (GFile                *file,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_3_32
void       ide_g_file_get_children_async            (GFile                *file,
                                                     const gchar          *attributes,
                                                     GFileQueryInfoFlags   flags,
                                                     gint                  io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_3_32
GPtrArray *ide_g_file_get_children_finish           (GFile                *file,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_3_32
gboolean   ide_g_host_file_get_contents             (const gchar          *path,
                                                     gchar               **contents,
                                                     gsize                *len,
                                                     GError              **error);

G_END_DECLS
