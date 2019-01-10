/* sysroot-plugin.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#include "config.h"

#include <libpeas/peas.h>
#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-sysroot-runtime-provider.h"
#include "gbp-sysroot-preferences-addin.h"
#include "gbp-sysroot-toolchain-provider.h"

_IDE_EXTERN void
_gbp_sysroot_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUNTIME_PROVIDER,
                                              GBP_TYPE_SYSROOT_RUNTIME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PREFERENCES_ADDIN,
                                              GBP_TYPE_SYSROOT_PREFERENCES_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TOOLCHAIN_PROVIDER,
                                              GBP_TYPE_SYSROOT_TOOLCHAIN_PROVIDER);
}
