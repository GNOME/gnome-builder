/* ide-debugger-fallbacks.c
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

#define G_LOG_DOMAIN "ide-debugger-fallbacks"

#include "config.h"

#include "ide-debugger.h"
#include "ide-debugger-private.h"

void
_ide_debugger_real_list_frames_async (IdeDebugger         *self,
                                      IdeDebuggerThread   *thread,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (!thread || IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_list_frames_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Listing stack frames is not supported");
}

GPtrArray *
_ide_debugger_real_list_frames_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
_ide_debugger_real_interrupt_async (IdeDebugger            *self,
                                    IdeDebuggerThreadGroup *thread_group,
                                    GCancellable           *cancellable,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (!thread_group || IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_interrupt_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Interrupting inferior is not supported");
}

gboolean
_ide_debugger_real_interrupt_finish (IdeDebugger   *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
_ide_debugger_real_send_signal_async (IdeDebugger         *self,
                                      gint                 signum,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_send_signal_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Sending signals to inferior is not supported");
}

gboolean
_ide_debugger_real_send_signal_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
_ide_debugger_real_modify_breakpoint_async (IdeDebugger                 *self,
                                            IdeDebuggerBreakpointChange  change,
                                            IdeDebuggerBreakpoint       *breakpoint,
                                            GCancellable                *cancellable,
                                            GAsyncReadyCallback          callback,
                                            gpointer                     user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT_CHANGE (change));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_modify_breakpoint_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Modifying breakpoints is not supported");
}

gboolean
_ide_debugger_real_modify_breakpoint_finish (IdeDebugger   *self,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
_ide_debugger_real_list_locals_async (IdeDebugger         *self,
                                      IdeDebuggerThread   *thread,
                                      IdeDebuggerFrame    *frame,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER_FRAME (frame));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_list_locals_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Listing locals is not supported");
}

GPtrArray *
_ide_debugger_real_list_locals_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
_ide_debugger_real_list_params_async (IdeDebugger         *self,
                                      IdeDebuggerThread   *thread,
                                      IdeDebuggerFrame    *frame,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER_FRAME (frame));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_list_locals_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Listing locals is not supported");
}

GPtrArray *
_ide_debugger_real_list_params_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
_ide_debugger_real_list_registers_async (IdeDebugger         *self,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_list_registers_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Listing registers is not supported");
}

GPtrArray *
_ide_debugger_real_list_registers_finish (IdeDebugger   *self,
                                          GAsyncResult  *result,
                                          GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
_ide_debugger_real_disassemble_async (IdeDebugger                   *self,
                                      const IdeDebuggerAddressRange *range,
                                      GCancellable                  *cancellable,
                                      GAsyncReadyCallback            callback,
                                      gpointer                       user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (range != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_disassemble_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Disassembly is not supported");
}

GPtrArray *
_ide_debugger_real_disassemble_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
_ide_debugger_real_interpret_async (IdeDebugger         *self,
                                    const gchar         *command,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (command != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           _ide_debugger_real_interpret_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Interpret command is not supported");
}

gboolean
_ide_debugger_real_interpret_finish (IdeDebugger   *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}
