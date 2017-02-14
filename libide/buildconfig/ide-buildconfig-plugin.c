/* ide-buildsystem-plugin.c
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "ide-buildsystem-plugin"

#include <libpeas/peas.h>

#include "buildconfig/ide-buildconfig-configuration-provider.h"
#include "buildconfig/ide-buildconfig-pipeline-addin.h"
#include "buildsystem/ide-build-pipeline-addin.h"
#include "buildsystem/ide-configuration-provider.h"

void
ide_buildconfig_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIGURATION_PROVIDER,
                                              IDE_TYPE_BUILDCONFIG_CONFIGURATION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                              IDE_TYPE_BUILDCONFIG_PIPELINE_ADDIN);
}
