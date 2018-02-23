/* ide-device.h
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
#include "ide-types.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_DEVICE_ERROR_NO_SUCH_DEVICE = 1,
} IdeDeviceError;

#define IDE_TYPE_DEVICE  (ide_device_get_type())
#define IDE_DEVICE_ERROR (ide_device_error_quark())

G_DECLARE_DERIVABLE_TYPE (IdeDevice, ide_device, IDE, DEVICE, IdeObject)

struct _IdeDeviceClass
{
  IdeObjectClass parent;

  const gchar *(*get_system_type)       (IdeDevice        *self);
  void         (*prepare_configuration) (IdeDevice        *self,
                                         IdeConfiguration *configuration);

  gpointer _reserved[32];
};

IDE_AVAILABLE_IN_ALL
GQuark       ide_device_error_quark           (void) G_GNUC_CONST;
IDE_AVAILABLE_IN_ALL
const gchar *ide_device_get_display_name      (IdeDevice        *self);
IDE_AVAILABLE_IN_ALL
void         ide_device_set_display_name      (IdeDevice        *self,
                                               const gchar      *display_name);
IDE_AVAILABLE_IN_ALL
const gchar *ide_device_get_icon_name         (IdeDevice        *self);
IDE_AVAILABLE_IN_ALL
void         ide_device_set_icon_name         (IdeDevice        *self,
                                               const gchar      *icon_name);
IDE_AVAILABLE_IN_ALL
const gchar *ide_device_get_id                (IdeDevice        *self);
IDE_AVAILABLE_IN_ALL
void         ide_device_set_id                (IdeDevice        *self,
                                               const gchar      *id);
IDE_AVAILABLE_IN_ALL
const gchar *ide_device_get_system_type       (IdeDevice        *self);
IDE_AVAILABLE_IN_ALL
void         ide_device_prepare_configuration (IdeDevice        *self,
                                               IdeConfiguration *configuration);

G_END_DECLS
