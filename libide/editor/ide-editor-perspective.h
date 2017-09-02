/* ide-editor-perspective.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "diagnostics/ide-source-location.h"
#include "editor/ide-editor-sidebar.h"
#include "editor/ide-editor-utilities.h"
#include "layout/ide-layout.h"
#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-transient-sidebar.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PERSPECTIVE (ide_editor_perspective_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorPerspective, ide_editor_perspective, IDE, EDITOR_PERSPECTIVE, IdeLayout)

void                       ide_editor_perspective_focus_buffer                  (IdeEditorPerspective *self,
                                                                                 IdeBuffer            *buffer);
void                       ide_editor_perspective_focus_buffer_in_current_stack (IdeEditorPerspective *self,
                                                                                 IdeBuffer            *buffer);
void                       ide_editor_perspective_focus_location                (IdeEditorPerspective *self,
                                                                                 IdeSourceLocation    *location);
IdeLayoutView             *ide_editor_perspective_get_active_view               (IdeEditorPerspective *self);
IdeLayoutGrid             *ide_editor_perspective_get_grid                      (IdeEditorPerspective *self);
IdeEditorSidebar          *ide_editor_perspective_get_sidebar                   (IdeEditorPerspective *self);
IdeLayoutTransientSidebar *ide_editor_perspective_get_transient_sidebar         (IdeEditorPerspective *self);
GtkWidget                 *ide_editor_perspective_get_utilities                 (IdeEditorPerspective *self);
GtkWidget                 *ide_editor_perspective_get_overlay                   (IdeEditorPerspective *self);

G_END_DECLS
