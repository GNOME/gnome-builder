/* ide-clang-client.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_CLIENT (ide_clang_client_get_type())

G_DECLARE_FINAL_TYPE (IdeClangClient, ide_clang_client, IDE, CLANG_CLIENT, IdeObject)

void            ide_clang_client_call_async                (IdeClangClient       *self,
                                                            const gchar          *method,
                                                            GVariant             *params,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
gboolean        ide_clang_client_call_finish               (IdeClangClient       *self,
                                                            GAsyncResult         *result,
                                                            GVariant            **reply,
                                                            GError              **error);
void            ide_clang_client_index_file_async          (IdeClangClient       *self,
                                                            GFile                *file,
                                                            const gchar * const  *flags,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
GVariant       *ide_clang_client_index_file_finish         (IdeClangClient       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);
void            ide_clang_client_get_index_key_async       (IdeClangClient       *self,
                                                            GFile                *file,
                                                            const gchar * const  *flags,
                                                            guint                 line,
                                                            guint                 column,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
gchar          *ide_clang_client_get_index_key_finish      (IdeClangClient       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);
void            ide_clang_client_find_nearest_scope_async  (IdeClangClient       *self,
                                                            GFile                *file,
                                                            const gchar * const  *flags,
                                                            guint                 line,
                                                            guint                 column,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IdeSymbol      *ide_clang_client_find_nearest_scope_finish (IdeClangClient       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);
void            ide_clang_client_locate_symbol_async       (IdeClangClient       *self,
                                                            GFile                *file,
                                                            const gchar * const  *flags,
                                                            guint                 line,
                                                            guint                 column,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
IdeSymbol      *ide_clang_client_locate_symbol_finish      (IdeClangClient       *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);

G_END_DECLS
