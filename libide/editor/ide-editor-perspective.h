/* ide-editor-perspective.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_EDITOR_PERSPECTIVE_H
#define IDE_EDITOR_PERSPECTIVE_H

#include <dazzle.h>
#include <gtk/gtk.h>

#include "diagnostics/ide-source-location.h"
#include "sourceview/ide-source-view.h"
#include "workbench/ide-layout.h"
#include "workbench/ide-perspective.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PERSPECTIVE (ide_editor_perspective_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorPerspective, ide_editor_perspective, IDE, EDITOR_PERSPECTIVE, DzlDockOverlay)

void                 ide_editor_perspective_focus_location                 (IdeEditorPerspective   *self,
                                                                            IdeSourceLocation      *location);
void                 ide_editor_perspective_focus_buffer_in_current_stack  (IdeEditorPerspective   *self,
                                                                            IdeBuffer              *buffer);
GtkWidget           *ide_editor_perspective_get_active_view                (IdeEditorPerspective   *self);
IdeLayout           *ide_editor_perspective_get_layout                     (IdeEditorPerspective   *self);

GtkWidget           *ide_editor_perspective_get_center_widget              (IdeEditorPerspective   *self);
GtkWidget           *ide_editor_perspective_get_top_edge                   (IdeEditorPerspective   *self);
GtkWidget           *ide_editor_perspective_get_left_edge                  (IdeEditorPerspective   *self);
GtkWidget           *ide_editor_perspective_get_bottom_edge                (IdeEditorPerspective   *self);
GtkWidget           *ide_editor_perspective_get_right_edge                 (IdeEditorPerspective   *self);

DzlDockOverlayEdge  *ide_editor_perspective_get_overlay_edge               (IdeEditorPerspective   *self,
                                                                            GtkPositionType         position);

void                 ide_editor_perspective_show_spellchecker              (IdeEditorPerspective   *self,
                                                                            IdeSourceView          *source_view);
G_END_DECLS

#endif /* IDE_EDITOR_PERSPECTIVE_H */
