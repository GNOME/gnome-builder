/* ide-editor-view-addin.h
 *
 * Copyright © 2015-2017 Christian Hergert <christian@hergert.me>
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

#include "ide-version-macros.h"

#include "editor/ide-editor-view.h"
#include "layout/ide-layout-stack.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_VIEW_ADDIN (ide_editor_view_addin_get_type ())

G_DECLARE_INTERFACE (IdeEditorViewAddin, ide_editor_view_addin, IDE, EDITOR_VIEW_ADDIN, GObject)

struct _IdeEditorViewAddinInterface
{
  GTypeInterface parent;

  void (*load)               (IdeEditorViewAddin *self,
                              IdeEditorView      *view);
  void (*unload)             (IdeEditorViewAddin *self,
                              IdeEditorView      *view);
  void (*language_changed)   (IdeEditorViewAddin *self,
                              const gchar        *language_id);
  void (*stack_set)          (IdeEditorViewAddin *self,
                              IdeLayoutStack     *stack);
};

IDE_AVAILABLE_IN_ALL
void                ide_editor_view_addin_load                (IdeEditorViewAddin *self,
                                                               IdeEditorView      *view);
IDE_AVAILABLE_IN_ALL
void                ide_editor_view_addin_unload              (IdeEditorViewAddin *self,
                                                               IdeEditorView      *view);
IDE_AVAILABLE_IN_ALL
void                ide_editor_view_addin_stack_set           (IdeEditorViewAddin *self,
                                                               IdeLayoutStack     *stack);
IDE_AVAILABLE_IN_ALL
void                ide_editor_view_addin_language_changed    (IdeEditorViewAddin *self,
                                                               const gchar        *language_id);
IDE_AVAILABLE_IN_ALL
IdeEditorViewAddin *ide_editor_view_addin_find_by_module_name (IdeEditorView      *view,
                                                               const gchar        *module_name);

G_END_DECLS
