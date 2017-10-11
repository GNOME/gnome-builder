/* ide-mingw-device.c
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

#define G_LOG_DOMAIN "ide-mingw-device"

#include "ide-mingw-device.h"

struct _IdeMingwDevice
{
  IdeDevice  parent_instance;
  gchar     *system_type;
};

G_DEFINE_DYNAMIC_TYPE (IdeMingwDevice, ide_mingw_device, IDE_TYPE_DEVICE)

IdeDevice *
ide_mingw_device_new (IdeContext  *context,
                      const gchar *display_name,
                      const gchar *id,
                      const gchar *system_type)
{
  IdeMingwDevice *self;

  self = g_object_new (IDE_TYPE_MINGW_DEVICE,
                       "context", context,
                       "display-name", display_name,
                       "id", id,
                       NULL);

  self->system_type = g_strdup (system_type);

  return IDE_DEVICE (self);
}

static const gchar *
ide_mingw_device_get_system_type (IdeDevice *device)
{
  IdeMingwDevice *self = (IdeMingwDevice *)device;

  g_assert (IDE_IS_MINGW_DEVICE (self));

  return self->system_type;
}

static void
ide_mingw_device_finalize (GObject *object)
{
  IdeMingwDevice *self = (IdeMingwDevice *)object;

  g_clear_pointer (&self->system_type, g_free);

  G_OBJECT_CLASS (ide_mingw_device_parent_class)->finalize (object);
}

static void
ide_mingw_device_class_init (IdeMingwDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->finalize = ide_mingw_device_finalize;

  device_class->get_system_type = ide_mingw_device_get_system_type;
}

static void ide_mingw_device_class_finalize (IdeMingwDeviceClass *klass) { }
static void ide_mingw_device_init (IdeMingwDevice *self) { }

void
_ide_mingw_device_register_type (GTypeModule *module)
{
  ide_mingw_device_register_type (module);
}
