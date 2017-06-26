/* ide-editor-view.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_EDITOR_VIEW_H
#define IDE_EDITOR_VIEW_H

#include "buffers/ide-buffer.h"
#include "layout/ide-layout-view.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_VIEW (ide_editor_view_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorView, ide_editor_view, IDE, EDITOR_VIEW, IdeLayoutView)

IdeBuffer      *ide_editor_view_get_document              (IdeEditorView *self);
IdeSourceView  *ide_editor_view_get_active_source_view    (IdeEditorView *self);

G_END_DECLS

#endif /* IDE_EDITOR_VIEW_H */
