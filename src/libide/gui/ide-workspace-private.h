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

#include <libide-plugins.h>

#include "ide-grid.h"
#include "ide-panel-position.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

typedef struct
{
  PanelDock  *dock;
  IdeGrid    *grid;
  PanelPaned *start_area;
  PanelPaned *end_area;
  PanelPaned *bottom_area;
} IdeWorkspaceDock;

void                    _ide_workspace_class_bind_template_dock (GtkWidgetClass       *widget_class,
                                                                 goffset               struct_offset);
GList                  *_ide_workspace_get_mru_link             (IdeWorkspace         *self);
void                    _ide_workspace_add_page_mru             (IdeWorkspace         *self,
                                                                 GList                *mru_link);
void                    _ide_workspace_remove_page_mru          (IdeWorkspace         *self,
                                                                 GList                *mru_link);
void                    _ide_workspace_move_front_page_mru      (IdeWorkspace         *workspace,
                                                                 GList                *mru_link);
void                    _ide_workspace_set_context              (IdeWorkspace         *workspace,
                                                                 IdeContext           *context);
IdeExtensionSetAdapter *_ide_workspace_get_addins               (IdeWorkspace         *self);
gboolean                _ide_workspace_can_search               (IdeWorkspace         *self);
void                    _ide_workspace_begin_global_search      (IdeWorkspace         *self);
void                    _ide_workspace_add_widget               (IdeWorkspace         *workspace,
                                                                 PanelWidget          *widget,
                                                                 PanelPosition        *position,
                                                                 IdeWorkspaceDock     *dock);
PanelFrame             *_ide_workspace_find_frame               (IdeWorkspace         *workspace,
                                                                 PanelPosition        *position,
                                                                 IdeWorkspaceDock     *dock);
void                    _ide_workspace_set_shortcut_model       (IdeWorkspace         *self,
                                                                 GListModel           *shortcuts);
void                    _ide_workspace_agree_to_close_async     (IdeWorkspace         *self,
                                                                 IdeGrid              *grid,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean                _ide_workspace_agree_to_close_finish    (IdeWorkspace         *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
void                    _ide_workspace_save_session             (IdeWorkspace         *self,
                                                                 IdeSession           *session);
void                    _ide_workspace_save_session_simple      (IdeWorkspace         *self,
                                                                 IdeSession           *session,
                                                                 IdeWorkspaceDock     *dock);
void                    _ide_workspace_restore_session          (IdeWorkspace         *self,
                                                                 IdeSession           *session);
void                    _ide_workspace_restore_session_simple   (IdeWorkspace         *self,
                                                                 IdeSession           *session,
                                                                 IdeWorkspaceDock     *dock);
void                    _ide_workspace_set_ignore_size_setting  (IdeWorkspace         *self,
                                                                 gboolean              ignore_size_setting);
gboolean                _ide_workspace_adopt_widget             (IdeWorkspace        *workspace,
                                                                 PanelWidget         *widget,
                                                                 PanelDock           *dock);

G_END_DECLS
