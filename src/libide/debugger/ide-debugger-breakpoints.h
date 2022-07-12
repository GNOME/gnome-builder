/* ide-debugger-breakpoints.h
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

#include <libide-core.h>

#include "ide-debugger-breakpoint.h"
#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_BREAKPOINTS (ide_debugger_breakpoints_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeDebuggerBreakpoints, ide_debugger_breakpoints, IDE, DEBUGGER_BREAKPOINTS, GObject)

IDE_AVAILABLE_IN_ALL
GFile                 *ide_debugger_breakpoints_get_file      (IdeDebuggerBreakpoints *self);
IDE_AVAILABLE_IN_ALL
IdeDebuggerBreakMode   ide_debugger_breakpoints_get_line_mode (IdeDebuggerBreakpoints *self,
                                                               guint                   line);
IDE_AVAILABLE_IN_ALL
IdeDebuggerBreakpoint *ide_debugger_breakpoints_get_line      (IdeDebuggerBreakpoints *self,
                                                               guint                   line);
IDE_AVAILABLE_IN_ALL
void                   ide_debugger_breakpoints_foreach       (IdeDebuggerBreakpoints *self,
                                                               GFunc                   func,
                                                               gpointer                user_data);

G_END_DECLS
