/* gbp-deviced-device.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-deviced-device"

#include "gbp-deviced-device.h"

struct _GbpDevicedDevice
{
  IdeDevice   parent_instance;
  DevdDevice *device;
};

G_DEFINE_TYPE (GbpDevicedDevice, gbp_deviced_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_deviced_device_finalize (GObject *object)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;

  IDE_ENTRY;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (gbp_deviced_device_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gbp_deviced_device_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpDevicedDevice *self = GBP_DEVICED_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_deviced_device_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpDevicedDevice *self = GBP_DEVICED_DEVICE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_deviced_device_class_init (GbpDevicedDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_deviced_device_finalize;
  object_class->get_property = gbp_deviced_device_get_property;
  object_class->set_property = gbp_deviced_device_set_property;

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The underlying libdeviced device",
                         DEVD_TYPE_DEVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_deviced_device_init (GbpDevicedDevice *self)
{
}

GbpDevicedDevice *
gbp_deviced_device_new (IdeContext *context,
                        DevdDevice *device)
{
  g_autofree gchar *id = NULL;
  const gchar *name;
  const gchar *icon_name;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (DEVD_IS_DEVICE (device), NULL);

  id = g_strdup_printf ("deviced:%s", devd_device_get_id (device));
  name = devd_device_get_name (device);
  icon_name = devd_device_get_icon_name (device);

  return g_object_new (GBP_TYPE_DEVICED_DEVICE,
                       "id", id,
                       "context", context,
                       "device", device,
                       "display-name", name,
                       "icon-name", icon_name,
                       NULL);
}
