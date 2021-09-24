/* ide-gui-private.h
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

#include <adwaita.h>
#include <libpeas/peas.h>
#include <libpeas/peas-autocleanups.h>
#include <libpanel.h>

#include <libide-core.h>
#include <libide-projects.h>

#include "ide-command-manager.h"
#include "ide-header-bar.h"
#include "ide-notification-list-box-row-private.h"
#include "ide-notification-stack-private.h"
#include "ide-notification-view-private.h"
#include "ide-page.h"
#include "ide-primary-workspace.h"
#include "ide-shortcut-label-private.h"
#include "ide-workbench.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

void      _ide_command_manager_init_shortcuts   (IdeCommandManager   *self,
                                                 IdeWorkspace        *workspace);
void      _ide_command_manager_unload_shortcuts (IdeCommandManager   *self,
                                                 IdeWorkspace        *workspace);
void      _ide_command_manager_execute          (IdeCommandManager   *self,
                                                 IdeWorkspace        *workspace,
                                                 const gchar         *command);
gboolean  _ide_gtk_widget_action_is_stateful    (GtkWidget           *widget,
                                                 const gchar         *action_name);
void      _ide_primary_workspace_init_actions   (IdePrimaryWorkspace *self);
void      _ide_workspace_init_actions           (IdeWorkspace        *self);
GList    *_ide_workspace_get_mru_link           (IdeWorkspace        *self);
void      _ide_workspace_add_page_mru           (IdeWorkspace        *self,
                                                 GList               *mru_link);
void      _ide_workspace_remove_page_mru        (IdeWorkspace        *self,
                                                 GList               *mru_link);
void      _ide_workspace_move_front_page_mru    (IdeWorkspace        *workspace,
                                                 GList               *mru_link);
void      _ide_workspace_set_context            (IdeWorkspace        *workspace,
                                                 IdeContext          *context);
gboolean  _ide_workbench_is_last_workspace      (IdeWorkbench        *self,
                                                 IdeWorkspace        *workspace);
void      _ide_header_bar_init_shortcuts        (IdeHeaderBar        *self);
void      _ide_header_bar_show_menu             (IdeHeaderBar        *self);
void      _ide_gtk_progress_bar_start_pulsing   (GtkProgressBar      *progress);
void      _ide_gtk_progress_bar_stop_pulsing    (GtkProgressBar      *progress);
void      _ide_surface_set_fullscreen           (IdeSurface          *self,
                                                 gboolean             fullscreen);

G_END_DECLS
