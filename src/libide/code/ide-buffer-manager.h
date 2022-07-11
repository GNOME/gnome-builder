/* ide-buffer-manager.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-buffer.h"

G_BEGIN_DECLS

/**
 * IdeBufferOpenFlags:
 * @IDE_BUFFER_OPEN_FLAGS_NONE: No special processing will be performed
 * @IDE_BUFFER_OPEN_FLAGS_BACKGROUND: Open the document in the background (behind current view)
 * @IDE_BUFFER_OPEN_FLAGS_NO_VIEW: Open the document but do not create a new view for it
 * @IDE_BUFFER_OPEN_FLAGS_DISABLE_ADDINS: Disables any buffer addin for this buffer.
 *
 * The #IdeBufferOpenFlags enumeration is used to specify how a document should
 * be opened by the workbench. Plugins may want to have a bit of control over
 * where the document is opened, and this provides a some control over that.
 */
typedef enum
{
  IDE_BUFFER_OPEN_FLAGS_NONE           = 0,
  IDE_BUFFER_OPEN_FLAGS_BACKGROUND     = 1 << 0,
  IDE_BUFFER_OPEN_FLAGS_NO_VIEW        = 1 << 1,
  IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD   = 1 << 2,
  IDE_BUFFER_OPEN_FLAGS_DISABLE_ADDINS = 1 << 3,
} IdeBufferOpenFlags;

/**
 * IdeBufferForeachFunc:
 * @buffer: an #IdeBuffer
 * @user_data: closure data
 *
 * Callback prototype for ide_buffer_manager_foreach().
 */
typedef void (*IdeBufferForeachFunc) (IdeBuffer *buffer,
                                      gpointer   user_data);

#define IDE_TYPE_BUFFER_MANAGER (ide_buffer_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeBufferManager, ide_buffer_manager, IDE, BUFFER_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeBufferManager *ide_buffer_manager_from_context       (IdeContext            *context);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_foreach            (IdeBufferManager      *self,
                                                         IdeBufferForeachFunc   foreach_func,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_load_file_async    (IdeBufferManager      *self,
                                                         GFile                 *file,
                                                         IdeBufferOpenFlags     flags,
                                                         IdeNotification       *notif,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
IdeBuffer        *ide_buffer_manager_load_file_finish   (IdeBufferManager      *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_save_all_async     (IdeBufferManager      *self,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_buffer_manager_save_all_finish    (IdeBufferManager      *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_reload_all_async   (IdeBufferManager      *self,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_buffer_manager_reload_all_finish  (IdeBufferManager      *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);
IDE_AVAILABLE_IN_ALL
gboolean          ide_buffer_manager_has_file           (IdeBufferManager      *self,
                                                         GFile                 *file);
IDE_AVAILABLE_IN_ALL
IdeBuffer        *ide_buffer_manager_find_buffer        (IdeBufferManager      *self,
                                                         GFile                 *file);
IDE_AVAILABLE_IN_ALL
gssize            ide_buffer_manager_get_max_file_size  (IdeBufferManager      *self);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_set_max_file_size  (IdeBufferManager      *self,
                                                         gssize                 max_file_size);
IDE_AVAILABLE_IN_ALL
void              ide_buffer_manager_apply_edits_async  (IdeBufferManager      *self,
                                                         GPtrArray             *edits,
                                                         GCancellable          *cancellable,
                                                         GAsyncReadyCallback    callback,
                                                         gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean          ide_buffer_manager_apply_edits_finish (IdeBufferManager      *self,
                                                         GAsyncResult          *result,
                                                         GError               **error);

G_END_DECLS
