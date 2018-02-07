/* ide-cross-compilation-device-provider.c
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

struct _IdeCrossCompilationDeviceProvider
{
  IdeObject  parent_instance;

  GPtrArray *devices;

  guint      settled : 1;
};

static void device_provider_iface_init (IdeDeviceProviderInterface *iface);
static void context_loaded (IdeContext* context, gpointer user_data);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCrossCompilationDeviceProvider,
                                ide_cross_compilation_device_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_DEVICE_PROVIDER,
                                                       device_provider_iface_init))

enum {
  PROP_0,
  PROP_SETTLED,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static GPtrArray *
ide_cross_compilation_device_provider_get_devices (IdeDeviceProvider *provider)
{
  IdeCrossCompilationDeviceProvider *self = IDE_CROSS_COMPILATION_DEVICE_PROVIDER(provider);

  g_return_val_if_fail (IDE_IS_CROSS_COMPILATION_DEVICE_PROVIDER (self), NULL);

  return g_ptr_array_ref (self->devices);
}

static void
ide_cross_compilation_device_provider_constructed (GObject *object)
{
  IdeCrossCompilationDeviceProvider *self = IDE_CROSS_COMPILATION_DEVICE_PROVIDER(object);
  IdeContext *context;

  g_assert (IDE_IS_CROSS_COMPILATION_DEVICE_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT(object));
  g_signal_connect (context, "loaded", G_CALLBACK (context_loaded), self);

  self->settled = TRUE;
  g_object_notify_by_pspec (object, properties [PROP_SETTLED]);
}

static void
ide_cross_compilation_device_provider_get_property (GObject    *object,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
  IdeCrossCompilationDeviceProvider *self = IDE_CROSS_COMPILATION_DEVICE_PROVIDER(object);

  switch (prop_id)
    {
    case PROP_SETTLED:
      g_value_set_boolean (value, self->settled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_cross_compilation_device_provider_init (IdeCrossCompilationDeviceProvider *self)
{
  self->devices = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
ide_cross_compilation_device_provider_finalize (GObject *object)
{
  IdeCrossCompilationDeviceProvider *self = IDE_CROSS_COMPILATION_DEVICE_PROVIDER(object);

  g_clear_pointer (&self->devices, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_cross_compilation_device_provider_parent_class)->finalize (object);
}

static void
ide_cross_compilation_device_provider_class_init (IdeCrossCompilationDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_cross_compilation_device_provider_constructed;
  object_class->finalize = ide_cross_compilation_device_provider_finalize;
  object_class->get_property = ide_cross_compilation_device_provider_get_property;

  properties [PROP_SETTLED] =
    g_param_spec_boolean ("settled",
                          "Settled",
                          "Settled",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void ide_cross_compilation_device_provider_class_finalize (IdeCrossCompilationDeviceProviderClass *klass) {
  
}

static void
device_provider_iface_init (IdeDeviceProviderInterface *iface)
{
  iface->get_devices = ide_cross_compilation_device_provider_get_devices;
}

static void
context_loaded (IdeContext* context,
                gpointer    user_data)
{
  IdeDevice *device;
  IdeBuildSystem *build_system;
  IdeCrossCompilationDeviceProvider *self = IDE_CROSS_COMPILATION_DEVICE_PROVIDER(user_data);
  gchar *build_id;

  build_system = ide_context_get_build_system (context);
  build_id = ide_build_system_get_id (build_system);
  device = ide_cross_compilation_device_new (ide_object_get_context (IDE_OBJECT (self)),
                                             "ARM Device",
                                             "super-test",
                                             "x86_64-linux-gnu");
  g_ptr_array_add (self->devices, device);
  ide_device_provider_emit_device_added (IDE_DEVICE_PROVIDER (self), device);
}

void
_ide_cross_compilation_device_provider_register_type (GTypeModule *module)
{
  ide_cross_compilation_device_provider_register_type (module);
}
