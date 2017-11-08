/* ide-device-manager.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-version-macros.h"

#include "ide-object.h"
#include "devices/ide-device-manager.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEVICE_MANAGER (ide_device_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeDeviceManager, ide_device_manager, IDE, DEVICE_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
void       ide_device_manager_add_provider    (IdeDeviceManager  *self,
                                               IdeDeviceProvider *provider);
IDE_AVAILABLE_IN_ALL
GPtrArray *ide_device_manager_get_devices     (IdeDeviceManager  *self);
IDE_AVAILABLE_IN_ALL
gboolean   ide_device_manager_get_settled     (IdeDeviceManager  *self);
IDE_AVAILABLE_IN_ALL
void       ide_device_manager_remove_provider (IdeDeviceManager  *self,
                                               IdeDeviceProvider *provider);
IDE_AVAILABLE_IN_ALL
IdeDevice *ide_device_manager_get_device      (IdeDeviceManager  *self,
                                               const gchar       *device_id);

G_END_DECLS
