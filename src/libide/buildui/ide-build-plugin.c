/* ide-build-plugin.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-build-plugin"

#include "object-modules.h"

#include "buildui/ide-build-workbench-addin.h"
#include "buildui/ide-build-config-view-addin.h"
#include "config/ide-config-view-addin.h"
#include "workbench/ide-workbench-addin.h"

void
ide_build_tool_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              IDE_TYPE_BUILD_WORKBENCH_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIG_VIEW_ADDIN,
                                              IDE_TYPE_BUILD_CONFIG_VIEW_ADDIN);
}
