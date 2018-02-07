/* ide-cross-compilation-plugin.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.co.uk>
 * Copyright (C) 2018 Collabora Ltd.
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

#include "ide-cross-compilation-device-provider.h"
#include "ide-cross-compilation-device.h"
#include "ide-cross-compilation-preferences-addin.h"

#include <libpeas/peas.h>
#include <ide.h>

void _ide_cross_compilation_device_provider_register_type (GTypeModule *module);
void _ide_cross_compilation_device_register_type (GTypeModule *module);
void _ide_cross_compilation_preferences_addin_register_type (GTypeModule *module);

void
ide_cross_compilation_register_types (PeasObjectModule *module)
{
  _ide_cross_compilation_device_provider_register_type (G_TYPE_MODULE (module));
  _ide_cross_compilation_device_register_type (G_TYPE_MODULE (module));
  _ide_cross_compilation_preferences_addin_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DEVICE_PROVIDER,
                                              IDE_TYPE_CROSS_COMPILATION_DEVICE_PROVIDER);

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DEVICE,
                                              IDE_TYPE_CROSS_COMPILATION_DEVICE);

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PREFERENCES_ADDIN,
                                              IDE_TYPE_CROSS_COMPILATION_PREFERENCES_ADDIN);
}
