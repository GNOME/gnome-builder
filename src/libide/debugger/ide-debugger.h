/* ide-debugger.h
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
#include <libide-code.h>
#include <libide-foundry.h>

#include "ide-debugger-breakpoint.h"
#include "ide-debugger-frame.h"
#include "ide-debugger-instruction.h"
#include "ide-debugger-library.h"
#include "ide-debugger-register.h"
#include "ide-debugger-thread-group.h"
#include "ide-debugger-thread.h"
#include "ide-debugger-types.h"
#include "ide-debugger-variable.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER (ide_debugger_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebugger, ide_debugger, IDE, DEBUGGER, IdeObject)

struct _IdeDebuggerClass
{
  IdeObjectClass parent_class;

  /* Signals */

  void       (*log)                      (IdeDebugger                  *self,
                                          IdeDebuggerStream             stream,
                                          GBytes                       *content);
  void       (*thread_group_added)       (IdeDebugger                  *self,
                                          IdeDebuggerThreadGroup       *thread_group);
  void       (*thread_group_removed)     (IdeDebugger                  *self,
                                          IdeDebuggerThreadGroup       *thread_group);
  void       (*thread_group_started)     (IdeDebugger                  *self,
                                          IdeDebuggerThreadGroup       *thread_group);
  void       (*thread_group_exited)      (IdeDebugger                  *self,
                                          IdeDebuggerThreadGroup       *thread_group);
  void       (*thread_added)             (IdeDebugger                  *self,
                                          IdeDebuggerThread            *thread);
  void       (*thread_removed)           (IdeDebugger                  *self,
                                          IdeDebuggerThread            *thread);
  void       (*thread_selected)          (IdeDebugger                  *self,
                                          IdeDebuggerThread            *thread);
  void       (*breakpoint_added)         (IdeDebugger                  *self,
                                          IdeDebuggerBreakpoint        *breakpoint);
  void       (*breakpoint_removed)       (IdeDebugger                  *self,
                                          IdeDebuggerBreakpoint        *breakpoint);
  void       (*breakpoint_modified)      (IdeDebugger                  *self,
                                          IdeDebuggerBreakpoint        *breakpoint);
  void       (*running)                  (IdeDebugger                  *self);
  void       (*stopped)                  (IdeDebugger                  *self,
                                          IdeDebuggerStopReason         stop_reason,
                                          IdeDebuggerBreakpoint        *breakpoint);
  void       (*library_loaded)           (IdeDebugger                  *self,
                                          IdeDebuggerLibrary           *library);
  void       (*library_unloaded)         (IdeDebugger                  *self,
                                          IdeDebuggerLibrary           *library);

  /* Virtual Functions */

  gboolean   (*supports_run_command)     (IdeDebugger                  *self,
                                          IdePipeline                  *pipeline,
                                          IdeRunCommand                *run_command,
                                          int                          *priority);
  void       (*prepare_for_run)          (IdeDebugger                  *self,
                                          IdePipeline                  *pipeline,
                                          IdeRunContext                *run_context);
  gboolean   (*get_can_move)             (IdeDebugger                  *self,
                                          IdeDebuggerMovement           movement);
  void       (*move_async)               (IdeDebugger                  *self,
                                          IdeDebuggerMovement           movement,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           callback,
                                          gpointer                      user_data);
  gboolean   (*move_finish)              (IdeDebugger                  *self,
                                          GAsyncResult                 *result,
                                          GError                      **error);
  void       (*list_breakpoints_async)   (IdeDebugger                  *self,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           callback,
                                          gpointer                      user_data);
  GPtrArray *(*list_breakpoints_finish)  (IdeDebugger                  *self,
                                          GAsyncResult                 *result,
                                          GError                      **error);
  void       (*insert_breakpoint_async)  (IdeDebugger                  *self,
                                          IdeDebuggerBreakpoint        *breakpoint,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           callback,
                                          gpointer                      user_data);
  gboolean   (*insert_breakpoint_finish) (IdeDebugger                  *self,
                                          GAsyncResult                 *result,
                                          GError                      **error);
  void       (*remove_breakpoint_async)  (IdeDebugger                  *self,
                                          IdeDebuggerBreakpoint        *breakpoint,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           callback,
                                          gpointer                      user_data);
  gboolean   (*remove_breakpoint_finish) (IdeDebugger                  *self,
                                          GAsyncResult                 *result,
                                          GError                      **error);
  void       (*modify_breakpoint_async)  (IdeDebugger                  *self,
                                          IdeDebuggerBreakpointChange   change,
                                          IdeDebuggerBreakpoint        *breakpoint,
                                          GCancellable                 *cancellable,
                                          GAsyncReadyCallback           callback,
                                          gpointer                      user_data);
  gboolean   (*modify_breakpoint_finish) (IdeDebugger                  *self,
                                          GAsyncResult                 *result,
                                          GError                        **error);
  void       (*list_frames_async)        (IdeDebugger                    *self,
                                          IdeDebuggerThread              *thread,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  GPtrArray *(*list_frames_finish)       (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*interrupt_async)          (IdeDebugger                    *self,
                                          IdeDebuggerThreadGroup         *thread_group,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  gboolean   (*interrupt_finish)         (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*send_signal_async)        (IdeDebugger                    *self,
                                          int                             signum,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  gboolean   (*send_signal_finish)       (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*list_locals_async)        (IdeDebugger                    *self,
                                          IdeDebuggerThread              *thread,
                                          IdeDebuggerFrame               *frame,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  GPtrArray *(*list_locals_finish)       (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*list_params_async)        (IdeDebugger                    *self,
                                          IdeDebuggerThread              *thread,
                                          IdeDebuggerFrame               *frame,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  GPtrArray *(*list_params_finish)       (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*list_registers_async)     (IdeDebugger                    *self,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  GPtrArray *(*list_registers_finish)    (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*disassemble_async)        (IdeDebugger                    *self,
                                          const IdeDebuggerAddressRange  *range,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  GPtrArray *(*disassemble_finish)       (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
  void       (*interpret_async)          (IdeDebugger                    *self,
                                          const gchar                    *command,
                                          GCancellable                   *cancellable,
                                          GAsyncReadyCallback             callback,
                                          gpointer                        user_data);
  gboolean   (*interpret_finish)         (IdeDebugger                    *self,
                                          GAsyncResult                   *result,
                                          GError                        **error);
};

IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_supports_run_command      (IdeDebugger                    *self,
                                                           IdePipeline                    *pipeline,
                                                           IdeRunCommand                  *run_command,
                                                           int                            *priority);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_prepare_for_run           (IdeDebugger                    *self,
                                                           IdePipeline                    *pipeline,
                                                           IdeRunContext                  *run_context);
IDE_AVAILABLE_IN_ALL
GListModel        *ide_debugger_get_breakpoints           (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_debugger_get_display_name          (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_set_display_name          (IdeDebugger                    *self,
                                                           const gchar                    *display_name);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_get_is_running            (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_get_can_move              (IdeDebugger                    *self,
                                                           IdeDebuggerMovement             movement);
IDE_AVAILABLE_IN_ALL
GListModel        *ide_debugger_get_threads               (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
GListModel        *ide_debugger_get_thread_groups         (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
IdeDebuggerThread *ide_debugger_get_selected_thread       (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_disassemble_async         (IdeDebugger                    *self,
                                                           const IdeDebuggerAddressRange  *range,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_disassemble_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_insert_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_insert_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_interrupt_async           (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_interrupt_finish          (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_modify_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpointChange     change,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_modify_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_remove_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_remove_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_list_breakpoints_async    (IdeDebugger                    *self,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_list_breakpoints_finish   (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_list_frames_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_list_frames_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_list_locals_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           IdeDebuggerFrame               *frame,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_list_locals_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_list_params_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           IdeDebuggerFrame               *frame,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_list_params_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_list_registers_async      (IdeDebugger                    *self,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray         *ide_debugger_list_registers_finish     (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_move_async                (IdeDebugger                    *self,
                                                           IdeDebuggerMovement             movement,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_move_finish               (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_send_signal_async         (IdeDebugger                    *self,
                                                           int                             signum,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_send_signal_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_debugger_locate_binary_at_address  (IdeDebugger                    *self,
                                                           IdeDebuggerAddress              address);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_log                  (IdeDebugger                    *self,
                                                           IdeDebuggerStream               stream,
                                                           GBytes                         *content);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_group_added   (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_group_removed (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_group_started (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_group_exited  (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_added         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_removed       (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_thread_selected      (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_breakpoint_added     (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_breakpoint_modified  (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_breakpoint_removed   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_running              (IdeDebugger                    *self);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_stopped              (IdeDebugger                    *self,
                                                           IdeDebuggerStopReason           stop_reason,
                                                           IdeDebuggerBreakpoint          *breakpoint);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_library_loaded       (IdeDebugger                    *self,
                                                           IdeDebuggerLibrary             *library);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_emit_library_unloaded     (IdeDebugger                    *self,
                                                           IdeDebuggerLibrary             *library);
IDE_AVAILABLE_IN_ALL
void               ide_debugger_interpret_async           (IdeDebugger                    *self,
                                                           const gchar                    *command,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_debugger_interpret_finish          (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);

G_END_DECLS
