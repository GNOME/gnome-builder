/* ide-application-tool.h
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

#pragma once

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION_TOOL (ide_application_tool_get_type())

G_DECLARE_INTERFACE (IdeApplicationTool, ide_application_tool, IDE, APPLICATION_TOOL, GObject)

/**
 * IdeApplicationToolInterface:
 * @run_async: You must implement this virtual function as part of
 *   running your command line operation.
 * @run_finish: You must implement this virtual function as part of
 *   running your command line operation.
 *
 * Use this interface to implement a command-line tool for Builder that
 * can be accessed when running builder in command-line mode using
 * "gnome-builder -t cli".
 *
 * Since: 3.22
 */
struct _IdeApplicationToolInterface
{
  GTypeInterface parent_interface;

  void (*run_async)  (IdeApplicationTool   *self,
                      const gchar * const  *arguments,
                      GCancellable         *cancellable,
                      GAsyncReadyCallback   callback,
                      gpointer              user_data);
  gint (*run_finish) (IdeApplicationTool   *self,
                      GAsyncResult         *result,
                      GError              **error);
};

IDE_AVAILABLE_IN_ALL
void ide_application_tool_run_async  (IdeApplicationTool   *self,
                                      const gchar * const  *arguments,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gint ide_application_tool_run_finish (IdeApplicationTool   *self,
                                      GAsyncResult         *result,
                                      GError              **error);

G_END_DECLS
