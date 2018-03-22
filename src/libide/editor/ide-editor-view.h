/* ide-editor-view.h
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "buffers/ide-buffer.h"
#include "editor/ide-editor-search.h"
#include "layout/ide-layout-view.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_VIEW (ide_editor_view_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorView, ide_editor_view, IDE, EDITOR_VIEW, IdeLayoutView)

IDE_AVAILABLE_IN_ALL
IdeBuffer         *ide_editor_view_get_buffer                  (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
IdeSourceView     *ide_editor_view_get_view                    (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
IdeEditorSearch   *ide_editor_view_get_search                  (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_editor_view_get_language_id             (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_scroll_to_line              (IdeEditorView     *self,
                                                                guint              line);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_scroll_to_line_offset       (IdeEditorView     *self,
                                                                guint              line,
                                                                guint              line_offset);
IDE_AVAILABLE_IN_ALL
gboolean           ide_editor_view_get_auto_hide_map           (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_set_auto_hide_map           (IdeEditorView     *self,
                                                                gboolean           auto_hide_map);
IDE_AVAILABLE_IN_ALL
gboolean           ide_editor_view_get_show_map                (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_set_show_map                (IdeEditorView     *self,
                                                                gboolean           show_map);
IDE_AVAILABLE_IN_ALL
GtkSourceLanguage *ide_editor_view_get_language                (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_set_language                (IdeEditorView     *self,
                                                                GtkSourceLanguage *language);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_move_next_error             (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_move_previous_error         (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_move_next_search_result     (IdeEditorView     *self);
IDE_AVAILABLE_IN_ALL
void               ide_editor_view_move_previous_search_result (IdeEditorView     *self);

G_END_DECLS
