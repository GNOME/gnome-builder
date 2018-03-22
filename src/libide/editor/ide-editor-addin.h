/* ide-editor-addin.h
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

#include "ide-version-macros.h"

#include "editor/ide-editor-perspective.h"
#include "layout/ide-layout-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_ADDIN (ide_editor_addin_get_type())

G_DECLARE_INTERFACE (IdeEditorAddin, ide_editor_addin, IDE, EDITOR_ADDIN, GObject)

struct _IdeEditorAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)     (IdeEditorAddin       *self,
                    IdeEditorPerspective *perspective);
  void (*unload)   (IdeEditorAddin       *self,
                    IdeEditorPerspective *perspective);
  void (*view_set) (IdeEditorAddin       *self,
                    IdeLayoutView        *view);
};

IDE_AVAILABLE_IN_ALL
void ide_editor_addin_load     (IdeEditorAddin       *self,
                                IdeEditorPerspective *perspective);
IDE_AVAILABLE_IN_ALL
void ide_editor_addin_unload   (IdeEditorAddin       *self,
                                IdeEditorPerspective *perspective);
IDE_AVAILABLE_IN_ALL
void ide_editor_addin_view_set (IdeEditorAddin       *self,
                                IdeLayoutView        *view);

IDE_AVAILABLE_IN_ALL
IdeEditorAddin *ide_editor_addin_find_by_module_name (IdeEditorPerspective *editor,
                                                      const gchar          *module_name);

G_END_DECLS
