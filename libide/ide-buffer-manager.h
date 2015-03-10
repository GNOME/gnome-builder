/* ide-buffer-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_BUFFER_MANAGER_H
#define IDE_BUFFER_MANAGER_H

#include <gtk/gtk.h>
#include <gtksourceview/completion-providers/words/gtksourcecompletionwords.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER_MANAGER (ide_buffer_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeBufferManager, ide_buffer_manager, IDE, BUFFER_MANAGER, IdeObject)

void                      ide_buffer_manager_load_file_async     (IdeBufferManager     *self,
                                                                  IdeFile              *file,
                                                                  gboolean              force_reload,
                                                                  IdeProgress         **progress,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
IdeBuffer                *ide_buffer_manager_load_file_finish    (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);
void                      ide_buffer_manager_save_file_async     (IdeBufferManager     *self,
                                                                  IdeBuffer            *buffer,
                                                                  IdeFile              *file,
                                                                  IdeProgress         **progress,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);
gboolean                  ide_buffer_manager_save_file_finish    (IdeBufferManager     *self,
                                                                  GAsyncResult         *result,
                                                                  GError              **error);
IdeBuffer                *ide_buffer_manager_get_focus_buffer    (IdeBufferManager     *self);
void                      ide_buffer_manager_set_focus_buffer    (IdeBufferManager     *self,
                                                                  IdeBuffer            *buffer);
GPtrArray                *ide_buffer_manager_get_buffers         (IdeBufferManager     *self);
GtkSourceCompletionWords *ide_buffer_manager_get_word_completion (IdeBufferManager     *self);

G_END_DECLS

#endif /* IDE_BUFFER_MANAGER_H */
