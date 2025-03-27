/*
 * gbp-arduino-device-provider.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-device-provider"

#include <json-glib/json-glib.h>

#include "gbp-arduino-device-provider.h"
#include "gbp-arduino-device-monitor.h"
#include "gbp-arduino-port.h"

struct _GbpArduinoDeviceProvider
{
  IdeDeviceProvider parent_instance;

  GbpArduinoDeviceMonitor *device_monitor;
};

G_DEFINE_FINAL_TYPE (GbpArduinoDeviceProvider, gbp_arduino_device_provider, IDE_TYPE_DEVICE_PROVIDER)

static void
on_port_added (GbpArduinoDeviceProvider *self,
               GbpArduinoPort           *port)
{
  g_assert (GBP_IS_ARDUINO_DEVICE_PROVIDER (self));
  g_assert (GBP_IS_ARDUINO_PORT (port));

  ide_device_provider_emit_device_added (IDE_DEVICE_PROVIDER (self), IDE_DEVICE (port));
}

static void
on_port_removed (GbpArduinoDeviceProvider *self,
                 GbpArduinoPort           *port)
{
  g_assert (GBP_IS_ARDUINO_DEVICE_PROVIDER (self));
  g_assert (GBP_IS_ARDUINO_PORT (port));

  ide_device_provider_emit_device_removed (IDE_DEVICE_PROVIDER (self), IDE_DEVICE (port));
}

static void
gbp_arduino_device_provider_load_async (IdeDeviceProvider  *provider,
                                        GCancellable       *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer            user_data)
{
  GbpArduinoDeviceProvider *self = (GbpArduinoDeviceProvider *) provider;
  g_autoptr (IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_DEVICE_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_arduino_device_provider_load_async);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_arduino_device_provider_load_finish (IdeDeviceProvider *provider,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_ARDUINO_DEVICE_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_arduino_device_provider_finalize (GObject *object)
{
  GbpArduinoDeviceProvider *self = (GbpArduinoDeviceProvider *) object;

  g_clear_object (&self->device_monitor);

  G_OBJECT_CLASS (gbp_arduino_device_provider_parent_class)->finalize (object);
}

static void
gbp_arduino_device_provider_class_init (GbpArduinoDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceProviderClass *provider_class = IDE_DEVICE_PROVIDER_CLASS (klass);

  object_class->finalize = gbp_arduino_device_provider_finalize;

  provider_class->load_async = gbp_arduino_device_provider_load_async;
  provider_class->load_finish = gbp_arduino_device_provider_load_finish;
}

static void
gbp_arduino_device_provider_init (GbpArduinoDeviceProvider *self)
{
  self->device_monitor = gbp_arduino_device_monitor_new ();

  gbp_arduino_device_monitor_start (self->device_monitor);

  g_signal_connect_object (self->device_monitor,
                           "added",
                           G_CALLBACK (on_port_added),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->device_monitor,
                           "removed",
                           G_CALLBACK (on_port_removed),
                           self,
                           G_CONNECT_SWAPPED);

}
