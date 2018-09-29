/* ide-debugger-private.h
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

#include "ide-debug-manager.h"
#include "ide-debugger.h"
#include "ide-debugger-breakpoints.h"

G_BEGIN_DECLS

void                    _ide_debug_manager_add_breakpoint           (IdeDebugManager                *self,
                                                                     IdeDebuggerBreakpoint          *breakpoint);
void                    _ide_debug_manager_remove_breakpoint        (IdeDebugManager                *self,
                                                                     IdeDebuggerBreakpoint          *breakpoint);
void                    _ide_debugger_breakpoint_reset              (IdeDebuggerBreakpoint          *self);
void                    _ide_debugger_breakpoints_add               (IdeDebuggerBreakpoints         *self,
                                                                     IdeDebuggerBreakpoint          *breakpoint);
void                    _ide_debugger_breakpoints_remove            (IdeDebuggerBreakpoints         *self,
                                                                     IdeDebuggerBreakpoint          *breakpoint);
void                    _ide_debugger_class_init_actions            (GActionGroupInterface          *iface);
void                    _ide_debugger_update_actions                (IdeDebugger                    *self);
gboolean                _ide_debugger_get_has_started               (IdeDebugger                    *self);
void                    _ide_debugger_real_list_frames_async        (IdeDebugger                    *self,
                                                                     IdeDebuggerThread              *thread,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
GPtrArray              *_ide_debugger_real_list_frames_finish       (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_interpret_async          (IdeDebugger                    *self,
                                                                     const gchar                    *command,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
gboolean                _ide_debugger_real_interpret_finish         (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_interrupt_async          (IdeDebugger                    *self,
                                                                     IdeDebuggerThreadGroup         *thread_group,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
gboolean                _ide_debugger_real_interrupt_finish         (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_send_signal_async        (IdeDebugger                    *self,
                                                                     gint                            signum,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
gboolean                _ide_debugger_real_send_signal_finish       (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_modify_breakpoint_async  (IdeDebugger                    *self,
                                                                     IdeDebuggerBreakpointChange     change,
                                                                     IdeDebuggerBreakpoint          *breakpoint,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
gboolean                _ide_debugger_real_modify_breakpoint_finish (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_list_params_async        (IdeDebugger                    *self,
                                                                     IdeDebuggerThread              *thread,
                                                                     IdeDebuggerFrame               *frame,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
GPtrArray              *_ide_debugger_real_list_params_finish       (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_list_locals_async        (IdeDebugger                    *self,
                                                                     IdeDebuggerThread              *thread,
                                                                     IdeDebuggerFrame               *frame,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
GPtrArray              *_ide_debugger_real_list_locals_finish       (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_list_registers_async     (IdeDebugger                    *self,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
GPtrArray              *_ide_debugger_real_list_registers_finish    (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);
void                    _ide_debugger_real_disassemble_async        (IdeDebugger                    *self,
                                                                     const IdeDebuggerAddressRange  *range,
                                                                     GCancellable                   *cancellable,
                                                                     GAsyncReadyCallback             callback,
                                                                     gpointer                        user_data);
GPtrArray              *_ide_debugger_real_disassemble_finish       (IdeDebugger                    *self,
                                                                     GAsyncResult                   *result,
                                                                     GError                        **error);

G_END_DECLS
