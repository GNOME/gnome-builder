/* ide-editor-page-addin.h
 *
 * Copyright 2015-2022 Christian Hergert <christian@hergert.me>
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

#include "ide-editor-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_PAGE_ADDIN (ide_editor_page_addin_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeEditorPageAddin, ide_editor_page_addin, IDE, EDITOR_PAGE_ADDIN, GObject)

struct _IdeEditorPageAddinInterface
{
  GTypeInterface parent;

  void          (*load)             (IdeEditorPageAddin *self,
                                     IdeEditorPage      *page);
  void          (*unload)           (IdeEditorPageAddin *self,
                                     IdeEditorPage      *page);
  void          (*language_changed) (IdeEditorPageAddin *self,
                                     const gchar        *language_id);
  void          (*frame_set)        (IdeEditorPageAddin *self,
                                     IdeFrame           *frame);
  GActionGroup *(*ref_action_group) (IdeEditorPageAddin *self);
};

IDE_AVAILABLE_IN_ALL
void                ide_editor_page_addin_load                (IdeEditorPageAddin *self,
                                                               IdeEditorPage      *page);
IDE_AVAILABLE_IN_ALL
void                ide_editor_page_addin_unload              (IdeEditorPageAddin *self,
                                                               IdeEditorPage      *page);
IDE_AVAILABLE_IN_ALL
void                ide_editor_page_addin_frame_set           (IdeEditorPageAddin *self,
                                                               IdeFrame           *frame);
IDE_AVAILABLE_IN_ALL
void                ide_editor_page_addin_language_changed    (IdeEditorPageAddin *self,
                                                               const gchar        *language_id);
IDE_AVAILABLE_IN_ALL
GActionGroup       *ide_editor_page_addin_ref_action_group    (IdeEditorPageAddin *self);
IDE_AVAILABLE_IN_ALL
IdeEditorPageAddin *ide_editor_page_addin_find_by_module_name (IdeEditorPage      *page,
                                                               const gchar        *module_name);

G_END_DECLS
