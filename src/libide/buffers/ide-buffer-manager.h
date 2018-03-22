/* ide-buffer-manager.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "ide-object.h"

#include "files/ide-file.h"
#include "workbench/ide-workbench.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER_MANAGER (ide_buffer_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeBufferManager, ide_buffer_manager, IDE, BUFFER_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeBuffer                *ide_buffer_manager_create_temporary_buffer
                                                                 (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_load_file_async     (IdeBufferManager     *self,
                                                                  IdeFile              *file,
                                                                  gboolean              force_reload,
                                                                  IdeWorkbenchOpenFlags flags,
                                                                  IdeProgress         **progress,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeBuffer                *ide_buffer_manager_load_file_finish    (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_save_file_async     (IdeBufferManager     *self,
                                                                  IdeBuffer            *buffer,
                                                                  IdeFile              *file,
                                                                  IdeProgress         **progress,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_manager_save_file_finish    (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_save_all_async      (IdeBufferManager     *self,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_manager_save_all_finish     (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
IdeBuffer                *ide_buffer_manager_get_focus_buffer    (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_set_focus_buffer    (IdeBufferManager     *self,
                                                                  IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
GPtrArray                *ide_buffer_manager_get_buffers         (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
GtkSourceCompletionWords *ide_buffer_manager_get_word_completion (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
guint                     ide_buffer_manager_get_n_buffers       (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_manager_has_file            (IdeBufferManager     *self,
                                                                  GFile                *file);
IDE_AVAILABLE_IN_ALL
IdeBuffer                *ide_buffer_manager_find_buffer         (IdeBufferManager     *self,
                                                                  GFile                *file);
IDE_AVAILABLE_IN_ALL
gsize                     ide_buffer_manager_get_max_file_size   (IdeBufferManager     *self);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_set_max_file_size   (IdeBufferManager     *self,
                                                                  gsize                 max_file_size);
IDE_AVAILABLE_IN_ALL
void                      ide_buffer_manager_apply_edits_async   (IdeBufferManager     *self,
                                                                  GPtrArray            *edits,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean                  ide_buffer_manager_apply_edits_finish  (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);

G_END_DECLS
