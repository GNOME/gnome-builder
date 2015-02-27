/* ide-device.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_DEVICE_H
#define IDE_DEVICE_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEVICE (ide_device_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDevice, ide_device, IDE, DEVICE, IdeObject)

struct _IdeDeviceClass
{
  IdeObjectClass parent;

  GKeyFile    *(*get_config)      (IdeDevice *device);
  const gchar *(*get_system_type) (IdeDevice *device);
};

GKeyFile    *ide_device_get_config       (IdeDevice   *device);
const gchar *ide_device_get_display_name (IdeDevice   *device);
void         ide_device_set_display_name (IdeDevice   *device,
                                          const gchar *display_name);
const gchar *ide_device_get_id           (IdeDevice   *device);
void         ide_device_set_id           (IdeDevice   *device,
                                          const gchar *id);
const gchar *ide_device_get_system_type  (IdeDevice   *device);

G_END_DECLS

#endif /* IDE_DEVICE_H */
