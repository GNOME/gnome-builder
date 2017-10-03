/* ide-device-provider.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include "devices/ide-device.h"
#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEVICE_PROVIDER (ide_device_provider_get_type())

G_DECLARE_INTERFACE (IdeDeviceProvider, ide_device_provider, IDE, DEVICE_PROVIDER, IdeObject)

struct _IdeDeviceProviderInterface
{
  GTypeInterface parent_interface;

  gboolean   (*get_settled) (IdeDeviceProvider *provider);
  GPtrArray *(*get_devices) (IdeDeviceProvider *provider);
};

void       ide_device_provider_emit_device_added   (IdeDeviceProvider *provider,
                                                    IdeDevice         *device);
void       ide_device_provider_emit_device_removed (IdeDeviceProvider *provider,
                                                    IdeDevice         *device);
GPtrArray *ide_device_provider_get_devices         (IdeDeviceProvider *provider);
gboolean   ide_device_provider_get_settled         (IdeDeviceProvider *provider);

G_END_DECLS
