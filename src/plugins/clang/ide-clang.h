/* ide-clang.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG (ide_clang_get_type())

G_DECLARE_FINAL_TYPE (IdeClang, ide_clang, IDE, CLANG, GObject)

IdeClang          *ide_clang_new                        (void);
void               ide_clang_set_workdir                (IdeClang             *self,
                                                         GFile                *workdir);
void               ide_clang_index_file_async           (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GPtrArray         *ide_clang_index_file_finish          (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_get_index_key_async        (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         guint                 line,
                                                         guint                 column,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gchar             *ide_clang_get_index_key_finish       (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_complete_async             (IdeClang             *self,
                                                         const gchar          *path,
                                                         guint                 line,
                                                         guint                 column,
                                                         const gchar * const  *argv,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GVariant          *ide_clang_complete_finish            (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_diagnose_async             (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GPtrArray         *ide_clang_diagnose_finish            (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_find_nearest_scope_async   (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         guint                 line,
                                                         guint                 column,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IdeSymbol         *ide_clang_find_nearest_scope_finish  (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_locate_symbol_async        (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         guint                 line,
                                                         guint                 column,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IdeSymbol         *ide_clang_locate_symbol_finish       (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_get_symbol_tree_async      (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
GVariant          *ide_clang_get_symbol_tree_finish     (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_get_highlight_index_async  (IdeClang             *self,
                                                         const gchar          *path,
                                                         const gchar * const  *argv,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IdeHighlightIndex *ide_clang_get_highlight_index_finish (IdeClang             *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
void               ide_clang_set_unsaved_file           (IdeClang             *self,
                                                         GFile                *file,
                                                         GBytes               *bytes);

G_END_DECLS
