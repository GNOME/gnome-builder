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

#include "ide-debug.h"

#include "debugger/ide-debugger.h"
#include "runner/ide-runner.h"

G_DEFINE_INTERFACE (IdeDebugger, ide_debugger, IDE_TYPE_OBJECT)

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
