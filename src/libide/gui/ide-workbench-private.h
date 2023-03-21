/* ide-workbench-private.h
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

#include <libpeas.h>

#include "ide-session.h"
#include "ide-workbench.h"
#include "ide-workspace.h"

G_BEGIN_DECLS

gboolean      _ide_workbench_is_last_workspace      (IdeWorkbench     *self,
                                                     IdeWorkspace     *workspace);
IdeWorkspace *_ide_workbench_create_secondary       (IdeWorkbench     *self);
void          _ide_workbench_addins_restore_session (IdeWorkbench     *self,
                                                     PeasExtensionSet *addins,
                                                     IdeSession       *session);
gboolean      _ide_workbench_restore_workspaces     (IdeWorkbench     *self,
                                                     IdeSession       *session,
                                                     gint64            present_time,
                                                     GType             expected_workspace);
void          _ide_workbench_set_session            (IdeWorkbench     *self,
                                                     IdeSession       *session);

G_END_DECLS
