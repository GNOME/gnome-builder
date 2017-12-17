/* meson-plugin.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include "gbp-meson-build-system.h"
#include "gbp-meson-build-target-provider.h"
#include "gbp-meson-pipeline-addin.h"
#include "gbp-meson-test-provider.h"

void
gbp_meson_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_PIPELINE_ADDIN, GBP_TYPE_MESON_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_SYSTEM, GBP_TYPE_MESON_BUILD_SYSTEM);
  peas_object_module_register_extension_type (module, IDE_TYPE_BUILD_TARGET_PROVIDER, GBP_TYPE_MESON_BUILD_TARGET_PROVIDER);
  peas_object_module_register_extension_type (module, IDE_TYPE_TEST_PROVIDER, GBP_TYPE_MESON_TEST_PROVIDER);
}
