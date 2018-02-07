/* ide-cross-compilation-device.c
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

#include "ide-cross-compilation-device.h"

struct _IdeCrossCompilationDevice
{
  IdeDevice  parent_instance;
  gchar     *system_type;
};

G_DEFINE_DYNAMIC_TYPE (IdeCrossCompilationDevice, ide_cross_compilation_device, IDE_TYPE_DEVICE)

IdeDevice *
ide_cross_compilation_device_new (IdeContext  *context,
                                  const gchar *display_name,
                                  const gchar *id,
                                  const gchar *system_type)
{
  IdeCrossCompilationDevice *self;

  self = g_object_new (IDE_TYPE_CROSS_COMPILATION_DEVICE,
                       "context", context,
                       "display-name", display_name,
                       "id", id,
                       NULL);

  self->system_type = g_strdup (system_type);

  return IDE_DEVICE (self);
}

static const gchar *
ide_cross_compilation_device_get_system_type (IdeDevice *device)
{
  IdeCrossCompilationDevice *self = IDE_CROSS_COMPILATION_DEVICE(device);

  g_assert (IDE_IS_CROSS_COMPILATION_DEVICE (self));

  return self->system_type;
}

void ide_cross_compilation_device_prepare_configuration (IdeDevice *self,
                                                         IdeConfiguration *configuration)
{
  g_critical("Here~~\n");
}

static void
ide_cross_compilation_device_init (IdeCrossCompilationDevice *self) {

}

static void
ide_cross_compilation_device_finalize (GObject *object)
{
  IdeCrossCompilationDevice *self = IDE_CROSS_COMPILATION_DEVICE(object);

  g_clear_pointer (&self->system_type, g_free);

  G_OBJECT_CLASS (ide_cross_compilation_device_parent_class)->finalize (object);
}

static void
ide_cross_compilation_device_class_init (IdeCrossCompilationDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->finalize = ide_cross_compilation_device_finalize;

  device_class->get_system_type = ide_cross_compilation_device_get_system_type;
  device_class->prepare_configuration = ide_cross_compilation_device_prepare_configuration;
}

static void ide_cross_compilation_device_class_finalize (IdeCrossCompilationDeviceClass *klass) {
  
}

void
_ide_cross_compilation_device_register_type (GTypeModule *module)
{
  ide_cross_compilation_device_register_type (module);
}

