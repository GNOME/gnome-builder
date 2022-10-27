/* ide-editor-page.h
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_EDITOR_INSIDE) && !defined (IDE_EDITOR_COMPILATION)
# error "Only <libide-editor.h> can be included directly."
#endif

#include <libide-core.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <libide-sourceview.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PAGE (ide_editor_page_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeEditorPage, ide_editor_page, IDE, EDITOR_PAGE, IdePage)

IDE_AVAILABLE_IN_ALL
GtkWidget     *ide_editor_page_new                       (IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
IdeBuffer     *ide_editor_page_get_buffer                (IdeEditorPage        *self);
IDE_AVAILABLE_IN_ALL
IdeSourceView *ide_editor_page_get_view                  (IdeEditorPage        *self);
IDE_AVAILABLE_IN_ALL
GFile         *ide_editor_page_get_file                  (IdeEditorPage        *self);
IDE_AVAILABLE_IN_ALL
IdeGutter     *ide_editor_page_get_gutter                (IdeEditorPage        *self);
IDE_AVAILABLE_IN_ALL
void           ide_editor_page_set_gutter                (IdeEditorPage        *self,
                                                          IdeGutter            *gutter);
IDE_AVAILABLE_IN_ALL
void           ide_editor_page_discard_changes_async     (IdeEditorPage        *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_editor_page_discard_changes_finish    (IdeEditorPage        *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_editor_page_save_async                (IdeEditorPage        *self,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean       ide_editor_page_save_finish               (IdeEditorPage        *self,
                                                          GAsyncResult         *result,
                                                          GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_editor_page_scroll_to_insert          (IdeEditorPage        *self,
                                                          GtkDirectionType      dir);
IDE_AVAILABLE_IN_ALL
void           ide_editor_page_scroll_to_visual_position (IdeEditorPage        *self,
                                                          guint                 line,
                                                          guint                 column);

G_END_DECLS
