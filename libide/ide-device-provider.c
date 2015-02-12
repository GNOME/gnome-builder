/* ide-device-provider.c
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

#include <glib/gi18n.h>

#include "ide-device-provider.h"

typedef struct
{
  GPtrArray *devices;
} IdeDeviceProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeDeviceProvider, ide_device_provider, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_SETTLED,
  LAST_PROP
};

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];
static GParamSpec *gParamSpecs [LAST_PROP];

gboolean
ide_device_provider_get_settled (IdeDeviceProvider *provider)
{
  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (provider), FALSE);

  if (IDE_DEVICE_PROVIDER_GET_CLASS (provider)->get_settled)
    return IDE_DEVICE_PROVIDER_GET_CLASS (provider)->get_settled (provider);

  return TRUE;
}

/**
 * ide_device_provider_get_devices:
 *
 * Retrieves a list of devices currently managed by @provider.
 *
 * Returns: (transfer none) (element-type IdeDevice*): A #GPtrArray of
 *  #IdeDevice instances.
 */
GPtrArray *
ide_device_provider_get_devices (IdeDeviceProvider *provider)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (provider);

  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (provider), NULL);

  return priv->devices;
}

static void
ide_device_provider_real_device_added (IdeDeviceProvider *provider,
                                       IdeDevice         *device)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (provider);

  g_ptr_array_add (priv->devices, g_object_ref (device));
}

static void
ide_device_provider_real_device_removed (IdeDeviceProvider *provider,
                                         IdeDevice         *device)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (provider);

  g_ptr_array_remove (priv->devices, device);
}

void
ide_device_provider_device_added (IdeDeviceProvider *provider,
                                  IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, gSignals [DEVICE_ADDED], 0, device);
}

void
ide_device_provider_device_removed (IdeDeviceProvider *provider,
                                    IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, gSignals [DEVICE_REMOVED], 0, device);
}

static void
ide_device_provider_finalize (GObject *object)
{
  IdeDeviceProvider *self = (IdeDeviceProvider *)object;
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (self);

  g_clear_pointer (&priv->devices, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_device_provider_parent_class)->finalize (object);
}

static void
ide_device_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeDeviceProvider *self = IDE_DEVICE_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_SETTLED:
      g_value_set_boolean (value, ide_device_provider_get_settled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_device_provider_class_init (IdeDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_device_provider_finalize;
  object_class->get_property = ide_device_provider_get_property;

  klass->device_added = ide_device_provider_real_device_added;
  klass->device_removed = ide_device_provider_real_device_removed;

  gParamSpecs [PROP_SETTLED] =
    g_param_spec_boolean ("settled",
                          _("Settled"),
                          _("If device probing has settled."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SETTLED,
                                   gParamSpecs [PROP_SETTLED]);

  gSignals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  IDE_TYPE_DEVICE_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDeviceProviderClass, device_added),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEVICE);

  gSignals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  IDE_TYPE_DEVICE_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDeviceProviderClass, device_removed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEVICE);
}

static void
ide_device_provider_init (IdeDeviceProvider *self)
{
  IdeDeviceProviderPrivate *priv;

  priv = ide_device_provider_get_instance_private (self);

  priv->devices = g_ptr_array_new_with_free_func (g_object_unref);
}
