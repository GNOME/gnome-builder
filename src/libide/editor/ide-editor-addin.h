/* ide-editor-addin.h
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

#include <libide-core.h>
#include <libide-gui.h>

#include "ide-editor-surface.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_ADDIN (ide_editor_addin_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeEditorAddin, ide_editor_addin, IDE, EDITOR_ADDIN, GObject)

struct _IdeEditorAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)     (IdeEditorAddin   *self,
                    IdeEditorSurface *surface);
  void (*unload)   (IdeEditorAddin   *self,
                    IdeEditorSurface *surface);
  void (*page_set) (IdeEditorAddin   *self,
                    IdePage          *page);
};

IDE_AVAILABLE_IN_3_32
void            ide_editor_addin_load                (IdeEditorAddin   *self,
                                                      IdeEditorSurface *surface);
IDE_AVAILABLE_IN_3_32
void            ide_editor_addin_unload              (IdeEditorAddin   *self,
                                                      IdeEditorSurface *surface);
IDE_AVAILABLE_IN_3_32
void            ide_editor_addin_page_set            (IdeEditorAddin   *self,
                                                      IdePage          *page);
IDE_AVAILABLE_IN_3_32
IdeEditorAddin *ide_editor_addin_find_by_module_name (IdeEditorSurface *editor,
                                                      const gchar      *module_name);

G_END_DECLS
