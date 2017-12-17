/* gbp-flatpak-plugin.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
#include <ide.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-build-system-discovery.h"
#include "gbp-flatpak-build-target-provider.h"
#include "gbp-flatpak-configuration-provider.h"
#include "gbp-flatpak-dependency-updater.h"
#include "gbp-flatpak-genesis-addin.h"
#include "gbp-flatpak-pipeline-addin.h"
#include "gbp-flatpak-preferences-addin.h"
#include "gbp-flatpak-runtime-provider.h"
#include "gbp-flatpak-workbench-addin.h"

void
gbp_flatpak_register_types (PeasObjectModule *module)
{
  ide_vcs_register_ignored (".flatpak-builder");

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                              GBP_TYPE_FLATPAK_BUILD_SYSTEM_DISCOVERY);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_TARGET_PROVIDER,
                                              GBP_TYPE_FLATPAK_BUILD_TARGET_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIGURATION_PROVIDER,
                                              GBP_TYPE_FLATPAK_CONFIGURATION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DEPENDENCY_UPDATER,
                                              GBP_TYPE_FLATPAK_DEPENDENCY_UPDATER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUNTIME_PROVIDER,
                                              GBP_TYPE_FLATPAK_RUNTIME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_FLATPAK_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_GENESIS_ADDIN,
                                              GBP_TYPE_FLATPAK_GENESIS_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                              GBP_TYPE_FLATPAK_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PREFERENCES_ADDIN,
                                              GBP_TYPE_FLATPAK_PREFERENCES_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_FLATPAK_WORKBENCH_ADDIN);
}
