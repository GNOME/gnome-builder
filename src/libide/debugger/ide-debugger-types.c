/* ide-debugger-types.c
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

#define G_LOG_DOMAIN "ide-debugger-types"

#include "config.h"

#include "ide-debugger-types.h"

GType
ide_debugger_stream_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_CONSOLE, "IDE_DEBUGGER_CONSOLE", "console" },
        { IDE_DEBUGGER_EVENT_LOG, "IDE_DEBUGGER_EVENT_LOG", "log" },
        { IDE_DEBUGGER_TARGET, "IDE_DEBUGGER_TARGET", "target" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerStream", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_debugger_movement_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_MOVEMENT_START, "IDE_DEBUGGER_MOVEMENT_START", "start" },
        { IDE_DEBUGGER_MOVEMENT_CONTINUE, "IDE_DEBUGGER_MOVEMENT_CONTINUE", "continue" },
        { IDE_DEBUGGER_MOVEMENT_STEP_IN, "IDE_DEBUGGER_MOVEMENT_STEP_IN", "step-in" },
        { IDE_DEBUGGER_MOVEMENT_STEP_OVER, "IDE_DEBUGGER_MOVEMENT_STEP_OUT", "step-out" },
        { IDE_DEBUGGER_MOVEMENT_FINISH, "IDE_DEBUGGER_MOVEMENT_FINISH", "finish" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerMovement", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_debugger_stop_reason_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_STOP_BREAKPOINT_HIT, "IDE_DEBUGGER_STOP_BREAKPOINT_HIT", "breakpoint-hit" },
        { IDE_DEBUGGER_STOP_CATCH, "IDE_DEBUGGER_STOP_CATCH", "catch" },
        { IDE_DEBUGGER_STOP_EXITED, "IDE_DEBUGGER_STOP_EXITED", "stop-exited" },
        { IDE_DEBUGGER_STOP_EXITED_NORMALLY, "IDE_DEBUGGER_STOP_EXITED_NORMALLY", "exited-normally" },
        { IDE_DEBUGGER_STOP_EXITED_SIGNALED, "IDE_DEBUGGER_STOP_EXITED_SIGNALED", "exited-signaled" },
        { IDE_DEBUGGER_STOP_FUNCTION_FINISHED, "IDE_DEBUGGER_STOP_FUNCTION_FINISHED", "function-finished" },
        { IDE_DEBUGGER_STOP_LOCATION_REACHED, "IDE_DEBUGGER_STOP_LOCATION_REACHED", "location-reached" },
        { IDE_DEBUGGER_STOP_SIGNAL_RECEIVED, "IDE_DEBUGGER_STOP_SIGNAL_RECEIVED", "signal-received" },
        { IDE_DEBUGGER_STOP_UNKNOWN, "IDE_DEBUGGER_STOP_UNKNOWN", "unknown" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerStopReason", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_debugger_break_mode_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_BREAK_NONE, "IDE_DEBUGGER_BREAK_NONE", "none" },
        { IDE_DEBUGGER_BREAK_BREAKPOINT, "IDE_DEBUGGER_BREAK_BREAKPOINT", "breakpoint" },
        { IDE_DEBUGGER_BREAK_COUNTPOINT, "IDE_DEBUGGER_BREAK_COUNTPOINT", "countpoint" },
        { IDE_DEBUGGER_BREAK_WATCHPOINT, "IDE_DEBUGGER_BREAK_WATCHPOINT", "watchpoint" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerBreakMode", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_debugger_disposition_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_DISPOSITION_KEEP, "IDE_DEBUGGER_DISPOSITION_KEEP", "keep" },
        { IDE_DEBUGGER_DISPOSITION_DISABLE, "IDE_DEBUGGER_DISPOSITION_DISABLE", "disable" },
        { IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT, "IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT", "delete-next-hit" },
        { IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP, "IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP", "delete-next-stop" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerDisposition", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
ide_debugger_breakpoint_change_get_type (void)
{
  static GType type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      static const GEnumValue values[] = {
        { IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED, "IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED", "enabled" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDebuggerBreakpointChange", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

G_DEFINE_BOXED_TYPE (IdeDebuggerAddressRange,
                     ide_debugger_address_range,
                     ide_debugger_address_range_copy,
                     ide_debugger_address_range_free)

IdeDebuggerAddressRange *
ide_debugger_address_range_copy (const IdeDebuggerAddressRange *range)
{
  return g_slice_dup (IdeDebuggerAddressRange, range);
}

void
ide_debugger_address_range_free (IdeDebuggerAddressRange *range)
{
  if (range != NULL)
    g_slice_free (IdeDebuggerAddressRange, range);
}

IdeDebuggerAddress
ide_debugger_address_parse (const gchar *string)
{
  if (string == NULL)
    return 0;

  if (g_str_has_prefix (string, "0x"))
    string += 2;

  return g_ascii_strtoull (string, NULL, 16);
}
