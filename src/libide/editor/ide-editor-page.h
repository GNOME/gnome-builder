/* ide-editor-page.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-code.h>
#include <libide-core.h>
#include <libide-gui.h>
#include <libide-sourceview.h>

#include "ide-editor-search.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PAGE (ide_editor_page_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeEditorPage, ide_editor_page, IDE, EDITOR_PAGE, IdePage)

IDE_AVAILABLE_IN_3_32
GFile             *ide_editor_page_get_file                    (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
IdeBuffer         *ide_editor_page_get_buffer                  (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
IdeSourceView     *ide_editor_page_get_view                    (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
IdeEditorSearch   *ide_editor_page_get_search                  (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
const gchar       *ide_editor_page_get_language_id             (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_scroll_to_line              (IdeEditorPage     *self,
                                                                guint              line);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_scroll_to_line_offset       (IdeEditorPage     *self,
                                                                guint              line,
                                                                guint              line_offset);
IDE_AVAILABLE_IN_3_32
gboolean           ide_editor_page_get_auto_hide_map           (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_set_auto_hide_map           (IdeEditorPage     *self,
                                                                gboolean           auto_hide_map);
IDE_AVAILABLE_IN_3_32
gboolean           ide_editor_page_get_show_map                (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_set_show_map                (IdeEditorPage     *self,
                                                                gboolean           show_map);
IDE_AVAILABLE_IN_3_32
GtkSourceLanguage *ide_editor_page_get_language                (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_set_language                (IdeEditorPage     *self,
                                                                GtkSourceLanguage *language);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_move_next_error             (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_move_previous_error         (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_move_next_search_result     (IdeEditorPage     *self);
IDE_AVAILABLE_IN_3_32
void               ide_editor_page_move_previous_search_result (IdeEditorPage     *self);

G_END_DECLS
