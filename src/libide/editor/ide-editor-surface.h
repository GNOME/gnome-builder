/* ide-editor-surface.h
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
# error "Only <idide-editor.h> can be included directly."
#endif

#include <dazzle.h>
#include <libide-code.h>
#include <libide-gui.h>

#include "ide-editor-sidebar.h"
#include "ide-editor-utilities.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SURFACE (ide_editor_surface_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeEditorSurface, ide_editor_surface, IDE, EDITOR_SURFACE, IdeSurface)

IDE_AVAILABLE_IN_3_32
IdeSurface          *ide_editor_surface_new                           (void);
IDE_AVAILABLE_IN_3_32
void                 ide_editor_surface_focus_buffer                  (IdeEditorSurface *self,
                                                                       IdeBuffer        *buffer);
IDE_AVAILABLE_IN_3_32
void                 ide_editor_surface_focus_buffer_in_current_stack (IdeEditorSurface *self,
                                                                       IdeBuffer        *buffer);
IDE_AVAILABLE_IN_3_32
void                 ide_editor_surface_focus_location                (IdeEditorSurface *self,
                                                                       IdeLocation      *location);
IDE_AVAILABLE_IN_3_32
IdePage             *ide_editor_surface_get_active_page               (IdeEditorSurface *self);
IDE_AVAILABLE_IN_3_32
IdeGrid             *ide_editor_surface_get_grid                      (IdeEditorSurface *self);
IDE_AVAILABLE_IN_3_32
IdeEditorSidebar    *ide_editor_surface_get_sidebar                   (IdeEditorSurface *self);
IDE_AVAILABLE_IN_3_32
IdeTransientSidebar *ide_editor_surface_get_transient_sidebar         (IdeEditorSurface *self);
IDE_AVAILABLE_IN_3_32
GtkWidget           *ide_editor_surface_get_utilities                 (IdeEditorSurface *self);
IDE_AVAILABLE_IN_3_32
GtkWidget           *ide_editor_surface_get_overlay                   (IdeEditorSurface *self);

G_END_DECLS
