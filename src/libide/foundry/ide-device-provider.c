/* ide-device-provider.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-device-provider"

#include "config.h"

#include "ide-device.h"
#include "ide-device-provider.h"

typedef struct
{
  GPtrArray *devices;
} IdeDeviceProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeDeviceProvider, ide_device_provider, IDE_TYPE_OBJECT)

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_device_provider_real_load_async (IdeDeviceProvider   *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_device_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_device_provider_real_load_finish (IdeDeviceProvider  *self,
                                      GAsyncResult       *result,
                                      GError            **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_device_provider_real_device_added (IdeDeviceProvider *self,
                                       IdeDevice         *device)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (self);

  g_assert (IDE_IS_DEVICE_PROVIDER (self));
  g_assert (IDE_IS_DEVICE (device));

  if (priv->devices == NULL)
    priv->devices = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->devices, g_object_ref (device));
}

static void
ide_device_provider_real_device_removed (IdeDeviceProvider *self,
                                         IdeDevice         *device)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (self);

  g_assert (IDE_IS_DEVICE_PROVIDER (self));
  g_assert (IDE_IS_DEVICE (device));

  /* Maybe we just disposed */
  if (priv->devices == NULL)
    return;

  if (!g_ptr_array_remove (priv->devices, device))
    g_warning ("No such device \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (device),
               G_OBJECT_TYPE_NAME (self));
}

static void
ide_device_provider_destroy (IdeObject *object)
{
  IdeDeviceProvider *self = (IdeDeviceProvider *)object;
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (self);

  g_clear_pointer (&priv->devices, g_ptr_array_unref);

  IDE_OBJECT_CLASS (ide_device_provider_parent_class)->destroy (object);
}

static void
ide_device_provider_class_init (IdeDeviceProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_device_provider_destroy;

  klass->device_added = ide_device_provider_real_device_added;
  klass->device_removed = ide_device_provider_real_device_removed;
  klass->load_async = ide_device_provider_real_load_async;
  klass->load_finish = ide_device_provider_real_load_finish;

  /**
   * IdeDeviceProvider::device-added:
   * @self: an #IdeDeviceProvider
   * @device: an #IdeDevice
   *
   * The "device-added" signal is emitted when a provider has discovered
   * a device has become available.
   *
   * Subclasses of #IdeDeviceManager must chain-up if they override the
   * #IdeDeviceProviderClass.device_added vfunc.
   */
  signals [DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDeviceProviderClass, device_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * IdeDeviceProvider::device-removed:
   * @self: an #IdeDeviceProvider
   * @device: an #IdeDevice
   *
   * The "device-removed" signal is emitted when a provider has discovered
   * a device is no longer available.
   *
   * Subclasses of #IdeDeviceManager must chain-up if they override the
   * #IdeDeviceProviderClass.device_removed vfunc.
   */
  signals [DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDeviceProviderClass, device_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_DEVICE);
  g_signal_set_va_marshaller (signals [DEVICE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
ide_device_provider_init (IdeDeviceProvider *self)
{
}

/**
 * ide_device_provider_emit_device_added:
 *
 * Emits the #IdeDeviceProvider::device-added signal.
 *
 * This should only be called by subclasses of #IdeDeviceProvider when
 * a new device has been discovered.
 */
void
ide_device_provider_emit_device_added (IdeDeviceProvider *provider,
                                       IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, signals [DEVICE_ADDED], 0, device);
}

/**
 * ide_device_provider_emit_device_removed:
 *
 * Emits the #IdeDeviceProvider::device-removed signal.
 *
 * This should only be called by subclasses of #IdeDeviceProvider when
 * a previously added device has been removed.
 */
void
ide_device_provider_emit_device_removed (IdeDeviceProvider *provider,
                                         IdeDevice         *device)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (provider));
  g_return_if_fail (IDE_IS_DEVICE (device));

  g_signal_emit (provider, signals [DEVICE_REMOVED], 0, device);
}

/**
 * ide_device_provider_load_async:
 * @self: an #IdeDeviceProvider
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests that the #IdeDeviceProvider asynchronously load any known devices.
 *
 * This should only be called once on an #IdeDeviceProvider. It is an error
 * to call this function more than once for a single #IdeDeviceProvider.
 *
 * #IdeDeviceProvider implementations are expected to emit the
 * #IdeDeviceProvider::device-added signal for each device they've discovered.
 * That should be done for known devices before returning from the asynchronous
 * operation so that the device manager does not need to wait for additional
 * devices to enter the "settled" state.
 */
void
ide_device_provider_load_async (IdeDeviceProvider   *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEVICE_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEVICE_PROVIDER_GET_CLASS (self)->load_async (self, cancellable, callback, user_data);
}

/**
 * ide_device_provider_load_finish:
 * @self: an #IdeDeviceProvider
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to load known devices via
 * ide_device_provider_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_device_provider_load_finish (IdeDeviceProvider  *self,
                                 GAsyncResult       *result,
                                 GError            **error)
{
  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEVICE_PROVIDER_GET_CLASS (self)->load_finish (self, result, error);
}

/**
 * ide_device_provider_get_devices:
 * @self: an #IdeDeviceProvider
 *
 * Gets a new #GPtrArray containing a list of #IdeDevice instances that were
 * registered by the #IdeDeviceProvider
 *
 * Returns: (transfer full) (element-type Ide.Device) (not nullable):
 *   a #GPtrArray of #IdeDevice.
 */
GPtrArray *
ide_device_provider_get_devices (IdeDeviceProvider *self)
{
  IdeDeviceProviderPrivate *priv = ide_device_provider_get_instance_private (self);
  g_autoptr(GPtrArray) devices = NULL;

  g_return_val_if_fail (IDE_IS_DEVICE_PROVIDER (self), NULL);

  devices = g_ptr_array_new ();

  if (priv->devices != NULL)
    {
      for (guint i = 0; i < priv->devices->len; i++)
        g_ptr_array_add (devices, g_object_ref (g_ptr_array_index (priv->devices, i)));
    }

  return g_steal_pointer (&devices);
}
