/*
 * arduino-plugin.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#include "config.h"

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-tweaks.h>
#include <libpeas.h>

#include "gbp-arduino-application-addin.h"
#include "gbp-arduino-build-system-discovery.h"
#include "gbp-arduino-build-system.h"
#include "gbp-arduino-config-provider.h"
#include "gbp-arduino-device-provider.h"
#include "gbp-arduino-pipeline-addin.h"
#include "gbp-arduino-template-provider.h"
#include "gbp-arduino-tweaks-addin.h"

_IDE_EXTERN void
_gbp_arduino_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_ARDUINO_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_SYSTEM_DISCOVERY,
                                              GBP_TYPE_ARDUINO_BUILD_SYSTEM_DISCOVERY);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUILD_SYSTEM,
                                              GBP_TYPE_ARDUINO_BUILD_SYSTEM);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CONFIG_PROVIDER,
                                              GBP_TYPE_ARDUINO_CONFIG_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DEVICE_PROVIDER,
                                              GBP_TYPE_ARDUINO_DEVICE_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PIPELINE_ADDIN,
                                              GBP_TYPE_ARDUINO_PIPELINE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TEMPLATE_PROVIDER,
                                              GBP_TYPE_ARDUINO_TEMPLATE_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_ARDUINO_TWEAKS_ADDIN);
}

