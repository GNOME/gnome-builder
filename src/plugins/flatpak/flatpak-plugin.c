/* flatpak-plugin.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "flatpak-plugin"

#include "config.h"

#include <libpeas.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-flatpak-aux.h"
#include "gbp-flatpak-build-system-discovery.h"
#include "gbp-flatpak-client.h"
#include "gbp-flatpak-config-provider.h"
#include "gbp-flatpak-dependency-updater.h"
#include "gbp-flatpak-pipeline-addin.h"
#include "gbp-flatpak-run-command-provider.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-sdk-provider.h"
#include "gbp-flatpak-tweaks-addin.h"
#include "gbp-flatpak-workbench-addin.h"

_IDE_EXTERN void
_gbp_flatpak_register_types (PeasObjectModule *module)
{
  ide_g_file_add_ignored_pattern (".flatpak-builder");

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                              GBP_TYPE_FLATPAK_BUILD_SYSTEM_DISCOVERY);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIG_PROVIDER,
                                              GBP_TYPE_FLATPAK_CONFIG_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DEPENDENCY_UPDATER,
                                              GBP_TYPE_FLATPAK_DEPENDENCY_UPDATER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PIPELINE_ADDIN,
                                              GBP_TYPE_FLATPAK_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUN_COMMAND_PROVIDER,
                                              GBP_TYPE_FLATPAK_RUN_COMMAND_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUNTIME_PROVIDER,
                                              GBP_TYPE_FLATPAK_RUNTIME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SDK_PROVIDER,
                                              GBP_TYPE_FLATPAK_SDK_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_FLATPAK_TWEAKS_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_FLATPAK_WORKBENCH_ADDIN);

  gbp_flatpak_aux_init ();

  /* Load the flatpak client early */
  (void)gbp_flatpak_client_get_default ();
}
