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

#include <libpeas/peas.h>

#include "gb-application-addin.h"
#include "gb-terminal-application-addin.h"
#include "gb-terminal-private.h"
#include "gb-terminal-resources.h"
#include "gb-terminal-workbench-addin.h"
#include "gb-workbench-addin.h"

void
peas_register_types (PeasObjectModule *module)
{
  _gb_terminal_application_addin_register_type (G_TYPE_MODULE (module));
  _gb_terminal_workbench_addin_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              GB_TYPE_APPLICATION_ADDIN,
                                              GB_TYPE_TERMINAL_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              GB_TYPE_WORKBENCH_ADDIN,
                                              GB_TYPE_TERMINAL_WORKBENCH_ADDIN);
}
