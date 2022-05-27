/* ide-debugger-breakpoint.h
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

#include "ide-debugger-frame.h"
#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_BREAKPOINT (ide_debugger_breakpoint_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerBreakpoint, ide_debugger_breakpoint, IDE, DEBUGGER_BREAKPOINT, GObject)

struct _IdeDebuggerBreakpointClass
{
  GObjectClass parent_class;

  void (*reset) (IdeDebuggerBreakpoint *self);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
gint                    ide_debugger_breakpoint_compare         (IdeDebuggerBreakpoint  *a,
                                                                 IdeDebuggerBreakpoint  *b);
IDE_AVAILABLE_IN_ALL
IdeDebuggerBreakpoint  *ide_debugger_breakpoint_new             (const gchar            *id);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_breakpoint_get_id          (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
gboolean                ide_debugger_breakpoint_get_enabled     (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_enabled     (IdeDebuggerBreakpoint  *self,
                                                                 gboolean                enabled);
IDE_AVAILABLE_IN_ALL
IdeDebuggerBreakMode    ide_debugger_breakpoint_get_mode        (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_mode        (IdeDebuggerBreakpoint  *self,
                                                                 IdeDebuggerBreakMode    mode);
IDE_AVAILABLE_IN_ALL
IdeDebuggerDisposition  ide_debugger_breakpoint_get_disposition (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_disposition (IdeDebuggerBreakpoint  *self,
                                                                 IdeDebuggerDisposition  disposition);
IDE_AVAILABLE_IN_ALL
IdeDebuggerAddress      ide_debugger_breakpoint_get_address     (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_address     (IdeDebuggerBreakpoint  *self,
                                                                 IdeDebuggerAddress      address);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_breakpoint_get_spec        (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_spec        (IdeDebuggerBreakpoint  *self,
                                                                 const gchar            *spec);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_breakpoint_get_function    (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_function    (IdeDebuggerBreakpoint  *self,
                                                                 const gchar            *function);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_breakpoint_get_file        (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_file        (IdeDebuggerBreakpoint  *self,
                                                                 const gchar            *file);
IDE_AVAILABLE_IN_ALL
guint                   ide_debugger_breakpoint_get_line        (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_line        (IdeDebuggerBreakpoint  *self,
                                                                 guint                   line);
IDE_AVAILABLE_IN_ALL
gint64                  ide_debugger_breakpoint_get_count       (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_count       (IdeDebuggerBreakpoint  *self,
                                                                 gint64                  count);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_breakpoint_get_thread      (IdeDebuggerBreakpoint  *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_breakpoint_set_thread      (IdeDebuggerBreakpoint  *self,
                                                                 const gchar            *thread);

G_END_DECLS
