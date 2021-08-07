/* ide-buffer-private.h
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

#include <libide-core.h>
#include <libide-plugins.h>

#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-highlight-engine.h"

G_BEGIN_DECLS

void                    _ide_buffer_manager_buffer_loaded    (IdeBufferManager     *self,
                                                              IdeBuffer            *buffer);
void                    _ide_buffer_manager_buffer_saved     (IdeBufferManager     *self,
                                                              IdeBuffer            *buffer);
void                    _ide_buffer_cancel_cursor_restore    (IdeBuffer            *self);
gboolean                _ide_buffer_can_restore_cursor       (IdeBuffer            *self);
IdeExtensionSetAdapter *_ide_buffer_get_addins               (IdeBuffer            *self);
IdeBuffer              *_ide_buffer_new                      (IdeBufferManager     *self,
                                                              GFile                *file,
                                                              gboolean              enable_addins,
                                                              gboolean              is_temporary);
void                    _ide_buffer_attach                   (IdeBuffer            *self,
                                                              IdeObject            *parent);
gboolean                _ide_buffer_is_file                  (IdeBuffer            *self,
                                                              GFile                *nolink_file);
void                    _ide_buffer_load_file_async          (IdeBuffer            *self,
                                                              IdeNotification      *notif,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
gboolean                _ide_buffer_load_file_finish         (IdeBuffer            *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
void                    _ide_buffer_line_flags_changed       (IdeBuffer            *self);
void                    _ide_buffer_set_changed_on_volume    (IdeBuffer            *self,
                                                              gboolean              changed_on_volume);
void                    _ide_buffer_set_read_only            (IdeBuffer            *self,
                                                              gboolean              read_only);
IdeHighlightEngine     *_ide_buffer_get_highlight_engine     (IdeBuffer            *self);
void                    _ide_buffer_set_failure              (IdeBuffer            *self,
                                                              const GError         *error);
void                    _ide_buffer_sync_to_unsaved_files    (IdeBuffer            *self);
void                    _ide_buffer_set_file                 (IdeBuffer            *self,
                                                              GFile                *file);
void                    _ide_buffer_request_scroll_to_cursor (IdeBuffer            *self);

G_END_DECLS
