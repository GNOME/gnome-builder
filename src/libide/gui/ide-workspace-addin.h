/* ide-workspace-addin.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-workspace.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKSPACE_ADDIN (ide_workspace_addin_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeWorkspaceAddin, ide_workspace_addin, IDE, WORKSPACE_ADDIN, GObject)

struct _IdeWorkspaceAddinInterface
{
  GTypeInterface parent_iface;

  void     (*load)        (IdeWorkspaceAddin *self,
                           IdeWorkspace      *workspace);
  void     (*unload)      (IdeWorkspaceAddin *self,
                           IdeWorkspace      *workspace);
  void     (*surface_set) (IdeWorkspaceAddin *self,
                           IdeSurface        *surface);
  gboolean (*can_close)   (IdeWorkspaceAddin *self);
};

IDE_AVAILABLE_IN_3_32
void ide_workspace_addin_load          (IdeWorkspaceAddin *self,
                                        IdeWorkspace      *workspace);
IDE_AVAILABLE_IN_3_32
void ide_workspace_addin_unload        (IdeWorkspaceAddin *self,
                                        IdeWorkspace      *workspace);
IDE_AVAILABLE_IN_3_32
void ide_workspace_addin_surface_set   (IdeWorkspaceAddin *self,
                                        IdeSurface        *surface);
IDE_AVAILABLE_IN_3_34
gboolean ide_workspace_addin_can_close (IdeWorkspaceAddin *self);

G_END_DECLS
