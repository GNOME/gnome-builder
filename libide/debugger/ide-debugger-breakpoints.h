/* ide-debugger-breakpoints.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#ifndef IDE_DEBUGGER_BREAKPOINTS_H
#define IDE_DEBUGGER_BREAKPOINTS_H

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_BREAKPOINTS (ide_debugger_breakpoints_get_type())

G_DECLARE_FINAL_TYPE (IdeDebuggerBreakpoints, ide_debugger_breakpoints, IDE, DEBUGGER_BREAKPOINTS, IdeObject)

typedef enum
{
  IDE_DEBUGGER_BREAK_NONE       = 0,
  IDE_DEBUGGER_BREAK_BREAKPOINT = 1 << 0,
  IDE_DEBUGGER_BREAK_COUNTPOINT = 1 << 1,
  IDE_DEBUGGER_BREAK_WATCHPOINT = 1 << 2,
} IdeDebuggerBreakType;

GFile                *ide_debugger_breakpoints_get_file (IdeDebuggerBreakpoints *self);
void                  ide_debugger_breakpoints_add      (IdeDebuggerBreakpoints *self,
                                                         guint                   line,
                                                         IdeDebuggerBreakType    break_type);
void                  ide_debugger_breakpoints_remove   (IdeDebuggerBreakpoints *self,
                                                         guint                   line);
IdeDebuggerBreakType  ide_debugger_breakpoints_lookup   (IdeDebuggerBreakpoints *self,
                                                         guint                   line);

G_END_DECLS

#endif /* IDE_DEBUGGER_BREAKPOINTS_H */
