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

#include "ide-frame.h"
#include "ide-header-bar.h"
#include "ide-page.h"
#include "ide-pane.h"
#include "ide-panel-position.h"
#include "ide-session.h"

G_BEGIN_DECLS

#define IDE_TYPE_WORKSPACE (ide_workspace_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeWorkspace, ide_workspace, IDE, WORKSPACE, AdwApplicationWindow)

typedef void (*IdeWorkspaceCallback) (IdeWorkspace *workspace,
                                      gpointer      user_data);

struct _IdeWorkspaceClass
{
  AdwApplicationWindowClass parent_class;

  IdeActionMixin action_mixin;

  const gchar *kind;

  guint _unused_flags : 32;

  void          (*context_set)           (IdeWorkspace         *self,
                                          IdeContext           *context);
  void          (*foreach_page)          (IdeWorkspace         *self,
                                          IdePageCallback       callback,
                                          gpointer              user_data);
  IdeHeaderBar *(*get_header_bar)        (IdeWorkspace         *self);
  IdePage      *(*get_most_recent_page)  (IdeWorkspace         *self);
  IdeFrame     *(*get_most_recent_frame) (IdeWorkspace         *self);
  void          (*agree_to_close_async)  (IdeWorkspace         *self,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
  gboolean      (*agree_to_close_finish) (IdeWorkspace         *self,
                                          GAsyncResult         *result,
                                          GError              **error);
  void          (*add_pane)              (IdeWorkspace         *self,
                                          IdePane              *pane,
                                          PanelPosition        *position);
  void          (*add_page)              (IdeWorkspace         *self,
                                          IdePage              *page,
                                          PanelPosition        *position);
  void          (*add_grid_column)       (IdeWorkspace         *self,
                                          guint                 column);
  void          (*add_overlay)           (IdeWorkspace         *self,
                                          GtkWidget            *overlay);
  void          (*remove_overlay)        (IdeWorkspace         *self,
                                          GtkWidget            *overlay);
  PanelFrame   *(*get_frame_at_position) (IdeWorkspace         *self,
                                          PanelPosition        *position);
  void          (*restore_size)          (IdeWorkspace         *self,
                                          int                   width,
                                          int                   height);
  gboolean      (*save_size)             (IdeWorkspace         *self,
                                          int                  *width,
                                          int                  *height);
  gboolean      (*can_search)            (IdeWorkspace         *self);
  void          (*save_session)          (IdeWorkspace         *self,
                                          IdeSession           *session);
  void          (*restore_session)       (IdeWorkspace         *self,
                                          IdeSession           *session);
  PanelStatusbar *(*get_statusbar)       (IdeWorkspace         *self);
};

IDE_AVAILABLE_IN_ALL
void            ide_workspace_class_install_action          (IdeWorkspaceClass     *workspace_class,
                                                             const char            *action_name,
                                                             const char            *parameter_type,
                                                             IdeActionActivateFunc  activate);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_class_install_property_action (IdeWorkspaceClass     *workspace_class,
                                                             const char            *action_name,
                                                             const char            *property_name);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_class_set_kind                (IdeWorkspaceClass     *klass,
                                                             const gchar           *kind);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_action_set_enabled            (IdeWorkspace          *self,
                                                             const char            *action_name,
                                                             gboolean               enabled);
IDE_AVAILABLE_IN_ALL
const char     *ide_workspace_get_id                        (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_set_id                        (IdeWorkspace          *self,
                                                             const char            *id);
IDE_AVAILABLE_IN_ALL
IdeHeaderBar   *ide_workspace_get_header_bar                (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
IdeContext     *ide_workspace_get_context                   (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
GCancellable   *ide_workspace_get_cancellable               (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_foreach_page                  (IdeWorkspace          *self,
                                                             IdePageCallback        callback,
                                                             gpointer               user_data);
IDE_AVAILABLE_IN_ALL
IdePage        *ide_workspace_get_most_recent_page          (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
IdeFrame       *ide_workspace_get_most_recent_frame         (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
PanelFrame     *ide_workspace_get_frame_at_position         (IdeWorkspace          *self,
                                                             PanelPosition         *position);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_add_pane                      (IdeWorkspace          *self,
                                                             IdePane               *pane,
                                                             PanelPosition         *position);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_add_page                      (IdeWorkspace          *self,
                                                             IdePage               *page,
                                                             PanelPosition         *position);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_add_grid_column               (IdeWorkspace          *self,
                                                             guint                  position);
IDE_AVAILABLE_IN_ALL
PanelStatusbar *ide_workspace_get_statusbar                 (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_add_overlay                   (IdeWorkspace          *self,
                                                             GtkWidget             *widget);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_remove_overlay                (IdeWorkspace          *self,
                                                             GtkWidget             *widget);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_inhibit_logout                (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_uninhibit_logout              (IdeWorkspace          *self);
IDE_AVAILABLE_IN_ALL
void            ide_workspace_set_toolbar_style            (IdeWorkspace           *self,
                                                            AdwToolbarStyle         style);
IDE_AVAILABLE_IN_ALL
AdwToolbarStyle ide_workspace_get_toolbar_style             (IdeWorkspace          *self);

G_END_DECLS
