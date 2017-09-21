/* gbp-create-project-plugin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>
#include <libpeas/peas.h>

#include "gbp-create-project-genesis-addin.h"
#include "gbp-create-project-tool.h"

void
peas_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_TOOL,
                                              GBP_TYPE_CREATE_PROJECT_TOOL);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_GENESIS_ADDIN,
                                              GBP_TYPE_CREATE_PROJECT_GENESIS_ADDIN);
}
