/* gbp-deviced-device-provider.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-deviced-device-provider"

#include <libdeviced.h>

#include "gbp-deviced-device.h"
#include "gbp-deviced-device-provider.h"

struct _GbpDevicedDeviceProvider
{
  IdeDeviceProvider parent_instance;

  /*
   * This is our browser for device nodes. As new nodes are discovered, we get
   * a signal which we translate into the IdeDviceProvider::device-added or
   * removed signals.
   */
  DevdBrowser *browser;
};

G_DEFINE_FINAL_TYPE (GbpDevicedDeviceProvider, gbp_deviced_device_provider, IDE_TYPE_DEVICE_PROVIDER)

static void
gbp_deviced_device_provider_device_added_cb (GbpDevicedDeviceProvider *self,
                                             DevdDevice               *device,
                                             DevdBrowser              *browser)
{
  GbpDevicedDevice *wrapped;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE_PROVIDER (self));
  g_assert (DEVD_IS_DEVICE (device));
  g_assert (DEVD_IS_BROWSER (browser));

  wrapped = gbp_deviced_device_new (device);
  g_object_set_data (G_OBJECT (device), "GBP_DEVICED_DEVICE", wrapped);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (wrapped));

  ide_device_provider_emit_device_added (IDE_DEVICE_PROVIDER (self), IDE_DEVICE (wrapped));

  IDE_EXIT;
}

static void
gbp_deviced_device_provider_device_removed_cb (GbpDevicedDeviceProvider *self,
                                               DevdDevice               *device,
                                               DevdBrowser              *browser)
{
  IdeDevice *wrapped;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE_PROVIDER (self));
  g_assert (DEVD_IS_DEVICE (device));
  g_assert (DEVD_IS_BROWSER (browser));

  if ((wrapped = g_object_get_data (G_OBJECT (device), "GBP_DEVICED_DEVICE")))
    ide_device_provider_emit_device_removed (IDE_DEVICE_PROVIDER (self), wrapped);

  g_object_set_data (G_OBJECT (device), "GBP_DEVICED_DEVICE", NULL);

  IDE_EXIT;
}

static void
gbp_deviced_device_provider_load_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  DevdBrowser *browser = (DevdBrowser *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!devd_browser_load_finish (browser, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_deviced_device_provider_load_async (IdeDeviceProvider   *provider,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevicedDeviceProvider *self = (GbpDevicedDeviceProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_device_provider_load_async);

  devd_browser_load_async (self->browser,
                           cancellable,
                           gbp_deviced_device_provider_load_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_deviced_device_provider_load_finish (IdeDeviceProvider  *provider,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_deviced_device_provider_finalize (GObject *object)
{
  GbpDevicedDeviceProvider *self = (GbpDevicedDeviceProvider *)object;

  g_clear_object (&self->browser);

  G_OBJECT_CLASS (gbp_deviced_device_provider_parent_class)->finalize (object);
}

static void
gbp_deviced_device_provider_class_init (GbpDevicedDeviceProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceProviderClass *provider_class = IDE_DEVICE_PROVIDER_CLASS (klass);

  object_class->finalize = gbp_deviced_device_provider_finalize;

  provider_class->load_async = gbp_deviced_device_provider_load_async;
  provider_class->load_finish = gbp_deviced_device_provider_load_finish;
}

static void
gbp_deviced_device_provider_init (GbpDevicedDeviceProvider *self)
{
  self->browser = devd_browser_new ();

  g_signal_connect_object (self->browser,
                           "device-added",
                           G_CALLBACK (gbp_deviced_device_provider_device_added_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->browser,
                           "device-removed",
                           G_CALLBACK (gbp_deviced_device_provider_device_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
