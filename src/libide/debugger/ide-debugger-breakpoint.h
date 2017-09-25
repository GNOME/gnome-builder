/* ide-debugger-breakpoint.h
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

#pragma once

#include <glib-object.h>

#include "ide-debugger-frame.h"
#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_BREAKPOINT (ide_debugger_breakpoint_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerBreakpoint, ide_debugger_breakpoint, IDE, DEBUGGER_BREAKPOINT, GObject)

struct _IdeDebuggerBreakpointClass
{
  GObjectClass parent_class;

  void (*reset) (IdeDebuggerBreakpoint *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

gint                    ide_debugger_breakpoint_compare          (IdeDebuggerBreakpoint  *a,
                                                                  IdeDebuggerBreakpoint  *b);
IdeDebuggerBreakpoint  *ide_debugger_breakpoint_new              (const gchar            *id);
const gchar            *ide_debugger_breakpoint_get_id           (IdeDebuggerBreakpoint  *self);
gboolean                ide_debugger_breakpoint_get_enabled      (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_enabled      (IdeDebuggerBreakpoint  *self,
                                                                  gboolean                enabled);
IdeDebuggerBreakMode    ide_debugger_breakpoint_get_mode         (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_mode         (IdeDebuggerBreakpoint  *self,
                                                                  IdeDebuggerBreakMode    mode);
IdeDebuggerDisposition  ide_debugger_breakpoint_get_disposition  (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_disposition  (IdeDebuggerBreakpoint  *self,
                                                                  IdeDebuggerDisposition  disposition);
IdeDebuggerAddress      ide_debugger_breakpoint_get_address      (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_address      (IdeDebuggerBreakpoint  *self,
                                                                  IdeDebuggerAddress      address);
const gchar            *ide_debugger_breakpoint_get_spec         (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_spec         (IdeDebuggerBreakpoint  *self,
                                                                  const gchar            *spec);
const gchar            *ide_debugger_breakpoint_get_function     (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_function     (IdeDebuggerBreakpoint *self,
                                                                  const gchar           *function);
const gchar            *ide_debugger_breakpoint_get_file         (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_file         (IdeDebuggerBreakpoint  *self,
                                                                  const gchar            *file);
guint                   ide_debugger_breakpoint_get_line         (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_line         (IdeDebuggerBreakpoint  *self,
                                                                  guint                   line);
gint64                  ide_debugger_breakpoint_get_count        (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_count        (IdeDebuggerBreakpoint  *self,
                                                                  gint64                  count);
const gchar            *ide_debugger_breakpoint_get_thread       (IdeDebuggerBreakpoint  *self);
void                    ide_debugger_breakpoint_set_thread       (IdeDebuggerBreakpoint  *self,
                                                                  const gchar            *thread);

G_END_DECLS
