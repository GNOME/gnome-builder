/* ide-device-provider.c
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "devices/ide-device-provider.h"

G_DEFINE_INTERFACE (IdeDeviceProvider, ide_device_provider, IDE_TYPE_OBJECT)

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static GPtrArray *
default_get_devices (IdeDeviceProvider *self)
{
  return g_ptr_array_new_with_free_func (g_object_unref);
}

static void
ide_device_provider_default_init (IdeDeviceProviderInterface *iface)
{
  iface->get_devices = default_get_devices;

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("settled",
                                                             "Settled",
                                                             "If the device provider has settled",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));

  signals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  IDE_TYPE_DEVICE_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEVICE);

  signals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  IDE_TYPE_DEVICE_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEVICE);
}

gboolean
ide_device_provider_get_settled (IdeDeviceProvider *provider)
{
  gboolean settled = FALSE;

  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (provider), FALSE);

  g_object_get (provider, "settled", &settled, NULL);

  return settled;
}

/**
 * ide_device_provider_get_devices:
 *
 * Retrieves a list of devices currently managed by @provider.
 *
 * Returns: (transfer container) (element-type Ide.Device): a #GPtrArray of
 *  #IdeDevice instances.
 */
GPtrArray *
ide_device_provider_get_devices (IdeDeviceProvider *provider)
{
  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (provider), NULL);

  return IDE_DEVICE_PROVIDER_GET_IFACE (provider)->get_devices (provider);
}

void
ide_device_provider_emit_device_added (IdeDeviceProvider *provider,
                                       IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, signals [DEVICE_ADDED], 0, device);
}

void
ide_device_provider_emit_device_removed (IdeDeviceProvider *provider,
                                         IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, signals [DEVICE_REMOVED], 0, device);
}
