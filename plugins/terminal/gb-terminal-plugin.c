/* gb-terminal-plugin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-application-addin.h"
#include "gb-plugins.h"
#include "gb-terminal-application-addin.h"
#include "gb-terminal-resources.h"
#include "gb-terminal-workbench-addin.h"

GB_DEFINE_EMBEDDED_PLUGIN (gb_terminal,
                           gb_terminal_get_resource (),
                           "resource:///org/gnome/builder/plugins/terminal/gb-terminal.plugin",
                           GB_DEFINE_PLUGIN_TYPE (GB_TYPE_APPLICATION_ADDIN, GB_TYPE_TERMINAL_APPLICATION_ADDIN)
                           GB_DEFINE_PLUGIN_TYPE (GB_TYPE_WORKBENCH_ADDIN, GB_TYPE_TERMINAL_WORKBENCH_ADDIN))
