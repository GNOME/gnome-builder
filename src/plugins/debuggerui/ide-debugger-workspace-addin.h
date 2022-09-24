/* ide-debugger-workspace-addin.h
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

#include <libide-debugger.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_WORKSPACE_ADDIN (ide_debugger_workspace_addin_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerWorkspaceAddin, ide_debugger_workspace_addin, IDE, DEBUGGER_WORKSPACE_ADDIN, GObject)

void ide_debugger_workspace_addin_raise_panel            (IdeDebuggerWorkspaceAddin *self);
void ide_debugger_workspace_addin_navigate_to_address    (IdeDebuggerWorkspaceAddin *self,
                                                          IdeDebuggerAddress         address);
void ide_debugger_workspace_addin_navigate_to_breakpoint (IdeDebuggerWorkspaceAddin *self,
                                                          IdeDebuggerBreakpoint     *breakpoint);
void ide_debugger_workspace_addin_navigate_to_file       (IdeDebuggerWorkspaceAddin *self,
                                                          GFile                     *file,
                                                          guint                      line);

G_END_DECLS
