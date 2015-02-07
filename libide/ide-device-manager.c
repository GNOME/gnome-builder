/* ide-device-manager.c
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

#include "ide-device.h"
#include "ide-device-manager.h"
#include "ide-device-provider.h"

#include "local/ide-local-device.h"

typedef struct
{
  GPtrArray *devices;
  GPtrArray *providers;
} IdeDeviceManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDeviceManager, ide_device_manager, IDE_TYPE_OBJECT)

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
ide_device_manager_get_settled (IdeDeviceManager *self)
{
  IdeDeviceManagerPrivate *priv;
  gsize i;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), FALSE);

  priv = ide_device_manager_get_instance_private (self);

  for (i = 0; i < priv->providers->len; i++)
    {
      IdeDeviceProvider *provider;

      provider = g_ptr_array_index (priv->providers, i);
      if (!ide_device_provider_get_settled (provider))
        return FALSE;
    }

  return TRUE;
}

static void
ide_device_manager_device_notify_settled (IdeDeviceManager  *self,
                                          GParamSpec        *pspec,
                                          IdeDeviceProvider *provider)
{
  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  g_object_notify_by_pspec (G_OBJECT (self),
                            gParamSpecs [PROP_SETTLED]);
}

static void
ide_device_manager_device_added (IdeDeviceManager  *self,
                                 IdeDevice         *device,
                                 IdeDeviceProvider *provider)
{
  IdeDeviceManagerPrivate *priv = ide_device_manager_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE (device));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  g_ptr_array_add (priv->devices, g_object_ref (device));

  g_signal_emit (self, gSignals [DEVICE_ADDED], 0, provider, device);
}

static void
ide_device_manager_device_removed (IdeDeviceManager  *self,
                                  IdeDevice         *device,
                                  IdeDeviceProvider *provider)
{
  IdeDeviceManagerPrivate *priv = ide_device_manager_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE (device));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_device_notify_settled),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_device_added),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_device_manager_device_removed),
                                        self);

  if (g_ptr_array_remove (priv->devices, device))
    g_signal_emit (self, gSignals [DEVICE_REMOVED], 0, provider, device);
}

void
ide_device_manager_add_provider (IdeDeviceManager  *self,
                                 IdeDeviceProvider *provider)
{
  IdeDeviceManagerPrivate *priv = ide_device_manager_get_instance_private (self);
  GPtrArray *devices;
  guint i;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));

  for (i = 0; i < priv->providers->len; i++)
    {
      if (provider == g_ptr_array_index (priv->providers, i))
        {
          g_warning ("Cannot add provider, already registered.");
          return;
        }
    }

  g_ptr_array_add (priv->providers, g_object_ref (provider));

  g_signal_connect_object (provider,
                           "notify::settled",
                           G_CALLBACK (ide_device_manager_device_notify_settled),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-added",
                           G_CALLBACK (ide_device_manager_device_added),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (provider,
                           "device-removed",
                           G_CALLBACK (ide_device_manager_device_removed),
                           self,
                           G_CONNECT_SWAPPED);

  devices = ide_device_provider_get_devices (provider);

  for (i = 0; i < devices->len; i++)
    {
      IdeDevice *device;

      device = g_ptr_array_index (devices, i);
      ide_device_manager_device_added (self, device, provider);
    }
}

/**
 * ide_device_manager_get_devices:
 *
 * Retrieves all of the devices that are registered with the #IdeDeviceManager.
 *
 * Returns: (transfer container) (element-type IdeDevice*): An array of devices
 *   registered with the #IdeManager.
 */
GPtrArray *
ide_device_manager_get_devices (IdeDeviceManager *self)
{
  IdeDeviceManagerPrivate *priv;
  GPtrArray *ret;
  guint i;

  g_return_val_if_fail (IDE_IS_DEVICE_MANAGER (self), NULL);

  priv = ide_device_manager_get_instance_private (self);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < priv->devices->len; i++)
    {
      IdeDevice *device;

      device = g_ptr_array_index (priv->devices, i);
      g_ptr_array_add (ret, g_object_ref (device));
    }

  return ret;
}

static void
ide_device_manager_add_local (IdeDeviceManager *manager)
{
  IdeDeviceManagerPrivate *priv;
  IdeContext *context;
  IdeDevice *device;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (manager));

  priv = ide_device_manager_get_instance_private (manager);

  context = ide_object_get_context (IDE_OBJECT (manager));
  device = g_object_new (IDE_TYPE_LOCAL_DEVICE,
                         "context", context,
                         NULL);
  g_ptr_array_add (priv->devices, g_object_ref (device));
  g_clear_object (&device);
}

static void
ide_device_manager_constructed (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;

  g_return_if_fail (IDE_IS_DEVICE_MANAGER (self));

  G_OBJECT_CLASS (ide_device_manager_parent_class)->constructed (object);

  ide_device_manager_add_local (self);
}

static void
ide_device_manager_finalize (GObject *object)
{
  IdeDeviceManager *self = (IdeDeviceManager *)object;
  IdeDeviceManagerPrivate *priv = ide_device_manager_get_instance_private (self);

  g_clear_pointer (&priv->devices, g_ptr_array_unref);
  g_clear_pointer (&priv->providers, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_device_manager_parent_class)->finalize (object);
}

static void
ide_device_manager_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeDeviceManager *self = IDE_DEVICE_MANAGER(object);

  switch (prop_id)
    {
    case PROP_SETTLED:
      g_value_set_boolean (value, ide_device_manager_get_settled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_device_manager_class_init (IdeDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_device_manager_constructed;
  object_class->finalize = ide_device_manager_finalize;
  object_class->get_property = ide_device_manager_get_property;

  gParamSpecs [PROP_SETTLED] =
    g_param_spec_boolean ("settled",
                          _("Settled"),
                          _("If the device providers have settled."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SETTLED,
                                   gParamSpecs [PROP_SETTLED]);

  gSignals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEVICE_PROVIDER,
                  IDE_TYPE_DEVICE);

  gSignals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEVICE_PROVIDER,
                  IDE_TYPE_DEVICE);
}

static void
ide_device_manager_init (IdeDeviceManager *self)
{
  IdeDeviceManagerPrivate *priv = ide_device_manager_get_instance_private (self);

  priv->devices = g_ptr_array_new_with_free_func (g_object_unref);
  priv->providers = g_ptr_array_new_with_free_func (g_object_unref);
}
