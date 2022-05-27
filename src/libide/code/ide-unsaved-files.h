/* ide-unsaved-files.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_UNSAVED_FILES (ide_unsaved_files_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeUnsavedFiles, ide_unsaved_files, IDE, UNSAVED_FILES, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeUnsavedFiles *ide_unsaved_files_from_context     (IdeContext           *context);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_update           (IdeUnsavedFiles      *self,
                                                     GFile                *file,
                                                     GBytes               *content);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_remove           (IdeUnsavedFiles      *self,
                                                     GFile                *file);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_save_async       (IdeUnsavedFiles      *files,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean         ide_unsaved_files_save_finish      (IdeUnsavedFiles      *files,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_restore_async    (IdeUnsavedFiles      *files,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean         ide_unsaved_files_restore_finish   (IdeUnsavedFiles      *files,
                                                     GAsyncResult         *result,
                                                     GError              **error);
IDE_AVAILABLE_IN_ALL
GPtrArray       *ide_unsaved_files_to_array         (IdeUnsavedFiles      *self);
IDE_AVAILABLE_IN_ALL
gint64           ide_unsaved_files_get_sequence     (IdeUnsavedFiles      *files);
IDE_AVAILABLE_IN_ALL
IdeUnsavedFile  *ide_unsaved_files_get_unsaved_file (IdeUnsavedFiles      *self,
                                                     GFile                *file);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_clear            (IdeUnsavedFiles      *self);
IDE_AVAILABLE_IN_ALL
gboolean         ide_unsaved_files_contains         (IdeUnsavedFiles      *self,
                                                     GFile                *file);
IDE_AVAILABLE_IN_ALL
void             ide_unsaved_files_reap_async       (IdeUnsavedFiles      *self,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean         ide_unsaved_files_reap_finish      (IdeUnsavedFiles      *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS
