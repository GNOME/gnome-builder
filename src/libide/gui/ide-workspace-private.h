/* ide-workspace-private.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include "ide-grid.h"
#include "ide-panel-position.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

void        _ide_workspace_init_actions        (IdeWorkspace        *self);
GList      *_ide_workspace_get_mru_link        (IdeWorkspace        *self);
void        _ide_workspace_add_page_mru        (IdeWorkspace        *self,
                                                GList               *mru_link);
void        _ide_workspace_remove_page_mru     (IdeWorkspace        *self,
                                                GList               *mru_link);
void        _ide_workspace_move_front_page_mru (IdeWorkspace        *workspace,
                                                GList               *mru_link);
void        _ide_workspace_set_context         (IdeWorkspace        *workspace,
                                                IdeContext          *context);
void        _ide_workspace_add_widget          (IdeWorkspace        *workspace,
                                                PanelWidget         *widget,
                                                IdePanelPosition    *position,
                                                PanelPaned          *dock_start,
                                                PanelPaned          *dock_end,
                                                PanelPaned          *dock_bottom,
                                                IdeGrid             *grid);
PanelFrame *_ide_workspace_find_frame          (IdeWorkspace        *workspace,
                                                IdePanelPosition    *position,
                                                PanelPaned          *dock_start,
                                                PanelPaned          *dock_end,
                                                PanelPaned          *dock_bottom,
                                                IdeGrid             *grid);

G_END_DECLS
