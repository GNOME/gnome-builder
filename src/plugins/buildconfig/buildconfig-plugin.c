/* buildconfig-plugin.c
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "buildconfig-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-foundry.h>

#include "ide-buildconfig-config-provider.h"
#include "ide-buildconfig-pipeline-addin.h"
#include "gbp-buildconfig-run-command-provider.h"

_IDE_EXTERN void
_gbp_buildconfig_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIG_PROVIDER,
                                              IDE_TYPE_BUILDCONFIG_CONFIG_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PIPELINE_ADDIN,
                                              IDE_TYPE_BUILDCONFIG_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUN_COMMAND_PROVIDER,
                                              GBP_TYPE_BUILDCONFIG_RUN_COMMAND_PROVIDER);
}
