/* ide-workspace.h
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <adwaita.h>

#include <libide-core.h>
#include <libide-projects.h>

#include "ide-header-bar.h"
#include "ide-page.h"
#include "ide-surface.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKSPACE (ide_workspace_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeWorkspace, ide_workspace, IDE, WORKSPACE, AdwApplicationWindow)

typedef void (*IdeWorkspaceCallback) (IdeWorkspace *workspace,
                                      gpointer      user_data);

struct _IdeWorkspaceClass
{
  AdwApplicationWindowClass parent_class;

  const gchar *kind;

  void (*context_set)  (IdeWorkspace    *self,
                        IdeContext      *context);
  void (*foreach_page) (IdeWorkspace    *self,
                        IdePageCallback  callback,
                        gpointer         user_data);
  void (*surface_set)  (IdeWorkspace    *self,
                        IdeSurface      *surface);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
void          ide_workspace_class_set_kind           (IdeWorkspaceClass *klass,
                                                      const gchar       *kind);
IDE_AVAILABLE_IN_ALL
IdeHeaderBar *ide_workspace_get_header_bar           (IdeWorkspace      *self);
IDE_AVAILABLE_IN_ALL
IdeContext   *ide_workspace_get_context              (IdeWorkspace      *self);
IDE_AVAILABLE_IN_ALL
GCancellable *ide_workspace_get_cancellable          (IdeWorkspace      *self);
IDE_AVAILABLE_IN_ALL
void          ide_workspace_foreach_page             (IdeWorkspace      *self,
                                                      IdePageCallback    callback,
                                                      gpointer           user_data);
IDE_AVAILABLE_IN_ALL
void          ide_workspace_foreach_surface          (IdeWorkspace       *self,
                                                      IdeSurfaceCallback  callback,
                                                      gpointer            user_data);
IDE_AVAILABLE_IN_ALL
void          ide_workspace_add_surface              (IdeWorkspace      *self,
                                                      IdeSurface        *surface);
IDE_AVAILABLE_IN_ALL
IdeSurface   *ide_workspace_get_surface_by_name      (IdeWorkspace      *self,
                                                      const gchar       *name);
IDE_AVAILABLE_IN_ALL
void          ide_workspace_set_visible_surface_name (IdeWorkspace      *self,
                                                      const gchar       *visible_surface_name);
IDE_AVAILABLE_IN_ALL
IdeSurface   *ide_workspace_get_visible_surface      (IdeWorkspace      *self);
IDE_AVAILABLE_IN_ALL
void          ide_workspace_set_visible_surface      (IdeWorkspace      *self,
                                                      IdeSurface        *surface);
IDE_AVAILABLE_IN_ALL
GtkOverlay   *ide_workspace_get_overlay              (IdeWorkspace      *self);
IDE_AVAILABLE_IN_ALL
IdePage      *ide_workspace_get_most_recent_page     (IdeWorkspace      *self);

G_END_DECLS
