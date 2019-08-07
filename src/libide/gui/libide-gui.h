/* ide-gui.h
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

#include <gtk/gtk.h>
#include <libide-core.h>
#include <libide-io.h>
#include <libide-projects.h>
#include <libide-threading.h>

#define IDE_GUI_INSIDE

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-cell-renderer-fancy.h"
#include "ide-command.h"
#include "ide-command-manager.h"
#include "ide-command-provider.h"
#include "ide-config-view-addin.h"
#include "ide-environment-editor.h"
#include "ide-frame.h"
#include "ide-frame-addin.h"
#include "ide-frame-empty-state.h"
#include "ide-frame-header.h"
#include "ide-header-bar.h"
#include "ide-fancy-tree-view.h"
#include "ide-grid.h"
#include "ide-grid-column.h"
#include "ide-gui-global.h"
#include "ide-header-bar.h"
#include "ide-marked-view.h"
#include "ide-notifications-button.h"
#include "ide-omni-bar-addin.h"
#include "ide-omni-bar.h"
#include "ide-page.h"
#include "ide-pane.h"
#include "ide-panel.h"
#include "ide-preferences-addin.h"
#include "ide-preferences-surface.h"
#include "ide-preferences-window.h"
#include "ide-primary-workspace.h"
#include "ide-search-button.h"
#include "ide-session-addin.h"
#include "ide-surface.h"
#include "ide-surfaces-button.h"
#include "ide-tagged-entry.h"
#include "ide-transfer-button.h"
#include "ide-transient-sidebar.h"
#include "ide-workbench.h"
#include "ide-workbench-addin.h"
#include "ide-workspace.h"
#include "ide-workspace-addin.h"

#undef IDE_GUI_INSIDE
