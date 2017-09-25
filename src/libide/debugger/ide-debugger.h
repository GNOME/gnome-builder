/* ide-debugger.h
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

#include <gio/gio.h>

#include "ide-object.h"

#include "debugger/ide-debugger-breakpoint.h"
#include "debugger/ide-debugger-frame.h"
#include "debugger/ide-debugger-instruction.h"
#include "debugger/ide-debugger-library.h"
#include "debugger/ide-debugger-register.h"
#include "debugger/ide-debugger-thread-group.h"
#include "debugger/ide-debugger-thread.h"
#include "debugger/ide-debugger-types.h"
#include "debugger/ide-debugger-variable.h"
#include "runner/ide-runner.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER (ide_debugger_get_type())

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

  gboolean   (*supports_runner)          (IdeDebugger                  *self,
                                          IdeRunner                    *runner,
                                          gint                         *priority);
  void       (*prepare)                  (IdeDebugger                  *self,
                                          IdeRunner                    *runner);
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
                                          gint                            signum,
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

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
  gpointer _reserved13;
  gpointer _reserved14;
  gpointer _reserved15;
  gpointer _reserved16;
  gpointer _reserved17;
  gpointer _reserved18;
  gpointer _reserved19;
  gpointer _reserved20;
  gpointer _reserved21;
  gpointer _reserved22;
  gpointer _reserved23;
  gpointer _reserved24;
  gpointer _reserved25;
  gpointer _reserved26;
  gpointer _reserved27;
  gpointer _reserved28;
  gpointer _reserved29;
  gpointer _reserved30;
  gpointer _reserved31;
  gpointer _reserved32;
};

gboolean           ide_debugger_supports_runner           (IdeDebugger                    *self,
                                                           IdeRunner                      *runner,
                                                           gint                           *priority);
void               ide_debugger_prepare                   (IdeDebugger                    *self,
                                                           IdeRunner                      *runner);
GListModel        *ide_debugger_get_breakpoints           (IdeDebugger                    *self);
const gchar       *ide_debugger_get_display_name          (IdeDebugger                    *self);
void               ide_debugger_set_display_name          (IdeDebugger                    *self,
                                                           const gchar                    *display_name);
gboolean           ide_debugger_get_is_running            (IdeDebugger                    *self);
gboolean           ide_debugger_get_can_move              (IdeDebugger                    *self,
                                                           IdeDebuggerMovement             movement);
GListModel        *ide_debugger_get_threads               (IdeDebugger                    *self);
GListModel        *ide_debugger_get_thread_groups         (IdeDebugger                    *self);
IdeDebuggerThread *ide_debugger_get_selected_thread       (IdeDebugger                    *self);
void               ide_debugger_disassemble_async         (IdeDebugger                    *self,
                                                           const IdeDebuggerAddressRange  *range,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_disassemble_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_insert_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_insert_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_interrupt_async           (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_interrupt_finish          (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_modify_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpointChange     change,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_modify_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_remove_breakpoint_async   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_remove_breakpoint_finish  (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_list_breakpoints_async    (IdeDebugger                    *self,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_list_breakpoints_finish   (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_list_frames_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_list_frames_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_list_locals_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           IdeDebuggerFrame               *frame,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_list_locals_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_list_params_async         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread,
                                                           IdeDebuggerFrame               *frame,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_list_params_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_list_registers_async      (IdeDebugger                    *self,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
GPtrArray         *ide_debugger_list_registers_finish     (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_move_async                (IdeDebugger                    *self,
                                                           IdeDebuggerMovement             movement,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_move_finish               (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
void               ide_debugger_send_signal_async         (IdeDebugger                    *self,
                                                           gint                            signum,
                                                           GCancellable                   *cancellable,
                                                           GAsyncReadyCallback             callback,
                                                           gpointer                        user_data);
gboolean           ide_debugger_send_signal_finish        (IdeDebugger                    *self,
                                                           GAsyncResult                   *result,
                                                           GError                        **error);
const gchar       *ide_debugger_locate_binary_at_address  (IdeDebugger                    *self,
                                                           IdeDebuggerAddress              address);
void               ide_debugger_emit_log                  (IdeDebugger                    *self,
                                                           IdeDebuggerStream               stream,
                                                           GBytes                         *content);
void               ide_debugger_emit_thread_group_added   (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
void               ide_debugger_emit_thread_group_removed (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
void               ide_debugger_emit_thread_group_started (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
void               ide_debugger_emit_thread_group_exited  (IdeDebugger                    *self,
                                                           IdeDebuggerThreadGroup         *thread_group);
void               ide_debugger_emit_thread_added         (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
void               ide_debugger_emit_thread_removed       (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
void               ide_debugger_emit_thread_selected      (IdeDebugger                    *self,
                                                           IdeDebuggerThread              *thread);
void               ide_debugger_emit_breakpoint_added     (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
void               ide_debugger_emit_breakpoint_modified  (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
void               ide_debugger_emit_breakpoint_removed   (IdeDebugger                    *self,
                                                           IdeDebuggerBreakpoint          *breakpoint);
void               ide_debugger_emit_running              (IdeDebugger                    *self);
void               ide_debugger_emit_stopped              (IdeDebugger                    *self,
                                                           IdeDebuggerStopReason           stop_reason,
                                                           IdeDebuggerBreakpoint          *breakpoint);
void               ide_debugger_emit_library_loaded       (IdeDebugger                    *self,
                                                           IdeDebuggerLibrary             *library);
void               ide_debugger_emit_library_unloaded     (IdeDebugger                    *self,
                                                           IdeDebuggerLibrary             *library);

G_END_DECLS
