/* ide-debugger.c
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

#define G_LOG_DOMAIN "ide-debugger"

#include "ide-enums.h"
#include "ide-debug.h"

#include "debugger/ide-debugger.h"
#include "diagnostics/ide-source-location.h"
#include "runner/ide-runner.h"

G_DEFINE_INTERFACE (IdeDebugger, ide_debugger, IDE_TYPE_OBJECT)

enum {
  LOG,
  STOPPED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

gchar *
ide_debugger_real_get_name (IdeDebugger *self)
{
  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_debugger_real_supports_runner (IdeDebugger *self,
                                   IdeRunner   *runner,
                                   gint        *priority)
{
  return FALSE;
}

static void
ide_debugger_default_init (IdeDebuggerInterface *iface)
{
  iface->get_name = ide_debugger_real_get_name;
  iface->supports_runner = ide_debugger_real_supports_runner;

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-step-in",
                                                             "Can Step In",
                                                             "If we can advance the debugger, stepping into any function call in the line",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-step-over",
                                                             "Can Step Over",
                                                             "If we can advance the debugger, stepping over any function calls in the line",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-continue",
                                                             "Can Continue",
                                                             "If we can advance the debugger to the next breakpoint",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * IdeDebugger:log:
   * @self: A #IdeDebugger
   * @message: the log message
   *
   * The "log" signal is emitted when the debugger has informative information
   * to display to the user.
   */
  signals [LOG] =
    g_signal_new ("log",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerInterface, log),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * IdeDebugger::stopped:
   * @self: An #IdeDebugger
   * @reason: An #IdeDebuggerStopReason for why the stop occurred
   * @location: An #IdeSourceLocation of where the debugger has stopped
   *
   * The "stopped" signal should be emitted when the debugger has stopped at a
   * new location. @reason indicates the reson for the stop, and @location is
   * the location where the stop has occurred.
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerInterface, stopped),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2, IDE_TYPE_DEBUGGER_STOP_REASON, IDE_TYPE_SOURCE_LOCATION);
}

/**
 * ide_debugger_supports_runner:
 * @self: An #IdeDebugger
 * @runner: An #IdeRunner
 * @priority: (out): A location to set the priority
 *
 * This function checks to see if the debugger supports the runner. This
 * allows the debugger to verify the program type or other necessary
 * dependency information.
 *
 * Returns: %TRUE if the @self supports @runner.
 */
gboolean
ide_debugger_supports_runner (IdeDebugger *self,
                              IdeRunner   *runner,
                              gint        *priority)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), FALSE);

  if (priority != NULL)
    *priority = G_MAXINT;

  ret = IDE_DEBUGGER_GET_IFACE (self)->supports_runner (self, runner, priority);

  IDE_TRACE_MSG ("Chceking if %s supports runner %s",
                 G_OBJECT_TYPE_NAME (self),
                 G_OBJECT_TYPE_NAME (runner));

  return ret;
}

/**
 * ide_debugger_get_name:
 * @self: A #IdeDebugger
 *
 * Gets the proper name of the debugger to display to the user.
 *
 * Returns: (transfer full): the display name for the debugger.
 */
gchar *
ide_debugger_get_name (IdeDebugger *self)
{
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  ret = IDE_DEBUGGER_GET_IFACE (self)->get_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return ret;
}

void
ide_debugger_emit_stopped (IdeDebugger           *self,
                           IdeDebuggerStopReason  reason,
                           IdeSourceLocation     *location)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  g_signal_emit (self, signals [STOPPED], 0, reason, location);
}

void
ide_debugger_prepare (IdeDebugger *self,
                      IdeRunner   *runner)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_RUNNER (runner));

  if (IDE_DEBUGGER_GET_IFACE (self)->prepare)
    IDE_DEBUGGER_GET_IFACE (self)->prepare (self, runner);
}

void
ide_debugger_run (IdeDebugger        *self,
                  IdeDebuggerRunType  run_type)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  if (IDE_DEBUGGER_GET_IFACE (self)->run)
    IDE_DEBUGGER_GET_IFACE (self)->run (self, run_type);
}

void
ide_debugger_emit_log (IdeDebugger *self,
                       const gchar *message)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  if (message != NULL)
    g_signal_emit (self, signals [LOG], 0, message);
}
