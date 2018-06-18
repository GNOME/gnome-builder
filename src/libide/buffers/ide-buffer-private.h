/* ide-buffer-private.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <libpeas/peas.h>

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "highlighting/ide-highlight-engine.h"

G_BEGIN_DECLS

PeasExtensionSet   *_ide_buffer_get_addins            (IdeBuffer        *self);
void                _ide_buffer_set_changed_on_volume (IdeBuffer        *self,
                                                       gboolean          changed_on_volume);
void                _ide_buffer_set_loading           (IdeBuffer        *self,
                                                       gboolean          loading);
void                _ide_buffer_cancel_cursor_restore (IdeBuffer        *self);
gboolean            _ide_buffer_can_restore_cursor    (IdeBuffer        *self);
void                _ide_buffer_set_mtime             (IdeBuffer        *self,
                                                       const GTimeVal   *mtime);
void                _ide_buffer_set_read_only         (IdeBuffer        *buffer,
                                                       gboolean          read_only);
void                _ide_buffer_set_failure           (IdeBuffer        *self,
                                                       const GError     *error);
IdeHighlightEngine *_ide_buffer_get_highlight_engine  (IdeBuffer        *self);
void                _ide_buffer_set_auto_save         (IdeBuffer        *self,
                                                       gboolean          auto_save,
                                                       guint             auto_save_timeout);
void                _ide_buffer_manager_reclaim       (IdeBufferManager *self,
                                                       IdeBuffer        *buffer);
IdeUnsavedFile     *_ide_unsaved_file_new             (GFile            *file,
                                                       GBytes           *content,
                                                       const gchar      *temp_path,
                                                       gint64            sequence);

G_END_DECLS
