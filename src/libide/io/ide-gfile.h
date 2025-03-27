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

/**
 * IdeFileWalkCallback:
 * @directory: a #GFile of the directory
 * @file_infos: (element-type GFileInfo): array of #GFileInfo children
 *   of @directory
 * @user_data: user data for callback
 *
 */
typedef void (*IdeFileWalkCallback) (GFile     *directory,
                                     GPtrArray *file_infos,
                                     gpointer   user_data);

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
gboolean   ide_path_is_ignored                      (const gchar          *path);
IDE_AVAILABLE_IN_ALL
gboolean   ide_g_file_is_ignored                    (GFile                *file);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_add_ignored_pattern           (const gchar          *pattern);
IDE_AVAILABLE_IN_ALL
gchar     *ide_g_file_get_uncanonical_relative_path (GFile                *file,
                                                     GFile                *other);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_g_file_find_with_depth               (GFile                *file,
                                                     const gchar          *pattern,
                                                     guint                 max_depth,
                                                     GCancellable         *cancellable);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_find_multiple_with_depth_async (GFile                *file,
                                                      const gchar * const  *patterns,
                                                      guint                 max_depth,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_find_with_depth_async         (GFile                *file,
                                                     const gchar          *pattern,
                                                     guint                 max_depth,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_find_async                    (GFile                *file,
                                                     const gchar          *pattern,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_g_file_find_finish                   (GFile                *file,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_get_children_async            (GFile                *file,
                                                     const gchar          *attributes,
                                                     GFileQueryInfoFlags   flags,
                                                     gint                  io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_g_file_get_children_finish           (GFile                *file,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
gboolean   ide_g_host_file_get_contents             (const gchar          *path,
                                                     gchar               **contents,
                                                     gsize                *len,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_walk                          (GFile                *directory,
                                                     const gchar          *attributes,
                                                     GCancellable         *cancellable,
                                                     IdeFileWalkCallback   callback,
                                                     gpointer              callback_data);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_walk_with_ignore              (GFile                *directory,
                                                     const gchar          *attributes,
                                                     const gchar          *ignore_file,
                                                     GCancellable         *cancellable,
                                                     IdeFileWalkCallback   callback,
                                                     gpointer              callback_data);
IDE_AVAILABLE_IN_ALL
void       ide_g_file_find_in_ancestors_async       (GFile                *directory,
                                                     const gchar          *name,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GFile     *ide_g_file_find_in_ancestors_finish      (GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS
