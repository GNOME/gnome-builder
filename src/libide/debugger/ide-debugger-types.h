/* ide-debugger-types.h
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

G_BEGIN_DECLS

/**
 * IdeDebuggerStream:
 * @IDE_DEBUGGER_TARGET: Logging from the inferior process
 * @IDE_DEBUGGER_CONSOLE: Logging from the debugger console
 * @IDE_DEBUGGER_EVENT_LOG: Internal event log from the debugger that can be
 *   used to troubleshoot the debugger.
 *
 * The type of stream for the log message.
 */
typedef enum
{
  IDE_DEBUGGER_TARGET,
  IDE_DEBUGGER_CONSOLE,
  IDE_DEBUGGER_EVENT_LOG,
} IdeDebuggerStream;

#define IDE_TYPE_DEBUGGER_STREAM  (ide_debugger_stream_get_type())
#define IDE_IS_DEBUGGER_STREAM(s) (((gint)s >= 0) && ((gint)s <= IDE_DEBUGGER_EVENT_LOG))

/**
 * IdeDebuggerMovement:
 * @IDE_DEBUGGER_MOVEMENT_START: Start or restart the application
 * @IDE_DEBUGGER_MOVEMENT_CONTINUE: Continue until a breakpoint is reached
 * @IDE_DEBUGGER_MOVEMENT_STEP_IN: Execute the next line of code, stepping into
 *   any function.
 * @IDE_DEBUGGER_MOVEMENT_STEP_OVER: Execute the next line of code, stepping over
 *   any function.
 * @IDE_DEBUGGER_MOVEMENT_FINISH: Run until the function returns.
 *
 * Describes the style of movement that should be performed by the debugger.
 */
typedef enum
{
  IDE_DEBUGGER_MOVEMENT_START,
  IDE_DEBUGGER_MOVEMENT_CONTINUE,
  IDE_DEBUGGER_MOVEMENT_STEP_IN,
  IDE_DEBUGGER_MOVEMENT_STEP_OVER,
  IDE_DEBUGGER_MOVEMENT_FINISH,
} IdeDebuggerMovement;

#define IDE_TYPE_DEBUGGER_MOVEMENT  (ide_debugger_movement_get_type())
#define IDE_IS_DEBUGGER_MOVEMENT(m) (((gint)m >= 0) && ((gint)m) <= IDE_DEBUGGER_MOVEMENT_FINISH)

/**
 * IdeDebuggerStopReason:
 * @IDE_DEBUGGER_STOP_BREAKPOINT: The debugger stopped because of a breakpoing
 * @IDE_DEBUGGER_STOP_EXITED_NORMALLY: The debugger stopped because the process exited
 *    in a graceful fashion.
 * @IDE_DEBUGGER_STOP_SIGNALED: The debugger stopped because the process
 *    received a death signal.
 *
 * Represents the reason a process has stopped executing in the debugger.
 */
typedef enum
{
  IDE_DEBUGGER_STOP_BREAKPOINT_HIT,
  IDE_DEBUGGER_STOP_EXITED,
  IDE_DEBUGGER_STOP_EXITED_NORMALLY,
  IDE_DEBUGGER_STOP_EXITED_SIGNALED,
  IDE_DEBUGGER_STOP_FUNCTION_FINISHED,
  IDE_DEBUGGER_STOP_LOCATION_REACHED,
  IDE_DEBUGGER_STOP_SIGNAL_RECEIVED,
  /* I think this can be used for a variety of catch positions in gdb,
   * and as a generic fallback for "this stopped, but not for the reason
   * of a particular breakpoint". Alternatively, a backend could insert
   * a transient breakpoint, stop on the breakpoint, and then remove it
   * after the stop event.
   */
  IDE_DEBUGGER_STOP_CATCH,
  IDE_DEBUGGER_STOP_UNKNOWN,
} IdeDebuggerStopReason;

#define IDE_TYPE_DEBUGGER_STOP_REASON    (ide_debugger_stop_reason_get_type())
#define IDE_IS_DEBUGGER_STOP_REASON(r)   (((gint)r >= 0) && ((gint)r) <= IDE_DEBUGGER_STOP_UNKNOWN)
#define IDE_DEBUGGER_STOP_IS_TERMINAL(r) (((r) == IDE_DEBUGGER_STOP_EXITED) || \
                                          ((r) == IDE_DEBUGGER_STOP_EXITED_NORMALLY) || \
                                          ((r) == IDE_DEBUGGER_STOP_EXITED_SIGNALED))

/**
 * IdeDebuggerBreakMode:
 * @IDE_DEBUGGER_BREAK_NONE: No breakpoint is set
 * @IDE_DEBUGGER_BREAK_BREAKPOINT: A simple breakpoint that stops the debugger
 *   when reaching a given location.
 * @IDE_DEBUGGER_BREAK_COUNTPOINT: A counter that is incremented when the
 *   debugger reaches a breakpoint.
 * @IDE_DEBUGGER_BREAK_WATCHPOINT: A breakpoint that is conditional on the
 *   specification matching.
 *
 * The type of breakpoint.
 */
typedef enum
{
  IDE_DEBUGGER_BREAK_NONE = 0,
  IDE_DEBUGGER_BREAK_BREAKPOINT,
  IDE_DEBUGGER_BREAK_COUNTPOINT,
  IDE_DEBUGGER_BREAK_WATCHPOINT,
} IdeDebuggerBreakMode;

#define IDE_TYPE_DEBUGGER_BREAK_MODE  (ide_debugger_break_mode_get_type())
#define IDE_IS_DEBUGGER_BREAK_MODE(m) (((gint)m >= 0) && ((gint)m) <= IDE_DEBUGGER_BREAK_WATCHPOINT)


/**
 * IdeDebuggerBreakpointChange:
 * @IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED: change the enabled state
 *
 * Describes the type of modification to perform on a breakpoint.
 */
typedef enum
{
  IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED = 1,
} IdeDebuggerBreakpointChange;

#define IDE_TYPE_DEBUGGER_BREAKPOINT_CHANGE  (ide_debugger_breakpoint_change_get_type())
#define IDE_IS_DEBUGGER_BREAKPOINT_CHANGE(c) (((gint)c > 0) && ((gint)c) <= IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED)


/**
 * IdeDebuggerDisposition:
 * @IDE_DEBUGGER_DISPOSITION_KEEP: the breakpoint will be kept after
 *   the next stop. This generally means the breakpoint is persistent until
 *   removed by the user.
 * @IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT: The breakpoint will be removed
 *   after the next time it is hit.
 * @IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP: The breakpoint will be removed
 *   the next time the debugger stops, even if not hit.
 * @IDE_DEBUGGER_DISPOSITION_DISABLE: The breakpoint is currently disabled.
 *
 * The disposition determines what should happen to the breakpoint at the next
 * stop of the debugger.
 */
typedef enum
{
  IDE_DEBUGGER_DISPOSITION_KEEP,
  IDE_DEBUGGER_DISPOSITION_DISABLE,
  IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT,
  IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP,
} IdeDebuggerDisposition;

#define IDE_TYPE_DEBUGGER_DISPOSITION  (ide_debugger_disposition_get_type())
#define IDE_IS_DEBUGGER_DISPOSITION(d) (((gint)d >= 0) && ((gint)d) <= IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP)


typedef guint64 IdeDebuggerAddress;

#define IDE_DEBUGGER_ADDRESS_INVALID (0)

IDE_AVAILABLE_IN_ALL
IdeDebuggerAddress ide_debugger_address_parse (const gchar *string);

typedef struct
{
  IdeDebuggerAddress from;
  IdeDebuggerAddress to;
} IdeDebuggerAddressRange;

#define IDE_TYPE_DEBUGGER_ADDRESS_RANGE (ide_debugger_address_range_get_type())


IDE_AVAILABLE_IN_ALL
GType ide_debugger_stream_get_type            (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_movement_get_type          (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_stop_reason_get_type       (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_break_mode_get_type        (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_disposition_get_type       (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_address_range_get_type     (void);
IDE_AVAILABLE_IN_ALL
GType ide_debugger_breakpoint_change_get_type (void);


IDE_AVAILABLE_IN_ALL
IdeDebuggerAddressRange *ide_debugger_address_range_copy (const IdeDebuggerAddressRange *range);
IDE_AVAILABLE_IN_ALL
void                     ide_debugger_address_range_free (IdeDebuggerAddressRange       *range);


G_END_DECLS
