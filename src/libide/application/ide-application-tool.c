/* ide-application-tool.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-application-tool"

#include "config.h"

#include "application/ide-application-tool.h"

G_DEFINE_INTERFACE (IdeApplicationTool, ide_application_tool, G_TYPE_OBJECT)

/**
 * SECTION:ide-application-tool
 * @title: IdeApplicationTool
 * @short_description: Implement command-line tools for Builder
 *
 * Builder supports running in "command-line" mode when executed like
 * "gnome-builder -t cli". If you use Builder from flatpak, this may be
 * activated like "flatpak run org.gnome.Builder -t cli".
 *
 * We suggest users that want to use this feature often to alias the
 * command in their shell such as "alias ide="gnome-builder -t cli".
 *
 * When running Builder in this mode, a number of command line tools can
 * be used instead of the full Gtk-based user interface.
 *
 * Plugins can implement this interface to provide additional command-line
 * tools. To be displayed in user help, you must also specify the name
 * such as "X-Tool-Name=foo" in your plugin's .plugin manifest. You should
 * also provide "X-Tool-Description=description" to notify the user of what
 * the tool does.
 *
 * Since: 3.22
 */

static void
ide_application_tool_default_init (IdeApplicationToolInterface *iface)
{
}

/**
 * ide_application_tool_run_async:
 * @self: An #IdeApplicationTool
 * @arguments: (array zero-terminated=1) (element-type utf8): argv for the command
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to execute upon completion
 * @user_data: User data for @callback
 *
 * Asynchronously runs an application tool. This is typically done on the
 * command line using the `ide` command.
 *
 * Since: 3.22
 */
void
ide_application_tool_run_async (IdeApplicationTool  *self,
                                const gchar * const *arguments,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_APPLICATION_TOOL (self));
  g_return_if_fail (arguments != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_APPLICATION_TOOL_GET_IFACE (self)->run_async (self, arguments, cancellable, callback, user_data);
}

/**
 * ide_application_tool_run_finish:
 * @self: a #IdeApplicationTool
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_application_tool_run_async().
 * This should return an exit code (were 0 is success) and set @error
 * when non-zero.
 *
 * The exit code shall be returned to the calling shell.
 *
 * Returns: A shell exit code, 0 for success, otherwise @error is set.
 *
 * Since: 3.22
 */
gint
ide_application_tool_run_finish (IdeApplicationTool  *self,
                                 GAsyncResult        *result,
                                 GError             **error)
{
  g_return_val_if_fail (IDE_IS_APPLICATION_TOOL (self), 0);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), 0);

  return IDE_APPLICATION_TOOL_GET_IFACE (self)->run_finish (self, result, error);
}
