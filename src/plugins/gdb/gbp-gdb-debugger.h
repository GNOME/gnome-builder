/* gbp-gdb-debugger.h
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "gdbwire.h"
#pragma GCC diagnostic pop

G_BEGIN_DECLS

#define GBP_TYPE_GDB_DEBUGGER (gbp_gdb_debugger_get_type())

G_DECLARE_FINAL_TYPE (GbpGdbDebugger, gbp_gdb_debugger, GBP, GDB_DEBUGGER, IdeDebugger)

GbpGdbDebugger           *gbp_gdb_debugger_new                (void);
void                      gbp_gdb_debugger_connect            (GbpGdbDebugger       *self,
                                                               GIOStream            *io_stream,
                                                               GCancellable         *cancellable);
void                      gbp_gdb_debugger_exec_async         (GbpGdbDebugger       *self,
                                                               const gchar          *command,
                                                               GCancellable         *cancellable,
                                                               GAsyncReadyCallback   callback,
                                                               gpointer              user_data);
struct gdbwire_mi_output *gbp_gdb_debugger_exec_finish        (GbpGdbDebugger       *self,
                                                               GAsyncResult         *result,
                                                               GError              **error);
void                      gbp_gdb_debugger_reload_breakpoints (GbpGdbDebugger       *self);


G_END_DECLS
