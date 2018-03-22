/* ide-local-device.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "devices/ide-device.h"

G_BEGIN_DECLS

#define IDE_TYPE_LOCAL_DEVICE (ide_local_device_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLocalDevice, ide_local_device, IDE, LOCAL_DEVICE, IdeDevice)

struct _IdeLocalDeviceClass
{
  IdeDeviceClass parent;

  /*< private >*/
  gpointer _reserved[8];
};

G_END_DECLS
