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
  IdeDevice      parent_instance;
  DevdDevice    *device;
  DevdClient    *client;
  IdeDeviceInfo *info;
  GQueue         connecting;
};

G_DEFINE_TYPE (GbpDevicedDevice, gbp_deviced_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_deviced_device_connect_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(GbpDevicedDevice) self = user_data;
  g_autoptr(GError) error = NULL;
  GList *list;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_DEVICED_DEVICE (self));

  list = g_steal_pointer (&self->connecting.head);
  self->connecting.head = NULL;
  self->connecting.length = 0;

  if (!devd_client_connect_finish (client, result, &error))
    {
      g_debug ("%s", error->message);
      g_clear_object (&self->client);

      for (const GList *iter = list; iter != NULL; iter = iter->next)
        {
          g_autoptr(GTask) task = iter->data;
          g_task_return_error (task, g_error_copy (error));
        }
    }
  else
    {
      for (const GList *iter = list; iter != NULL; iter = iter->next)
        {
          g_autoptr(GTask) task = iter->data;
          g_task_return_pointer (task, g_object_ref (client), g_object_unref);
        }
    }

  g_list_free (list);
}

static void
gbp_deviced_device_get_client_async (GbpDevicedDevice    *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_deviced_device_get_client_async);

  if (self->client != NULL && self->connecting.length == 0)
    {
      g_task_return_pointer (task, g_object_ref (self->client), g_object_unref);
      return;
    }

  g_queue_push_tail (&self->connecting, g_steal_pointer (&task));

  if (self->client == NULL)
    {
      self->client = devd_device_create_client (self->device);
      devd_client_connect_async (self->client,
                                 NULL,
                                 gbp_deviced_device_connect_cb,
                                 g_object_ref (self));
    }
}

static DevdClient *
gbp_deviced_device_get_client_finish (GbpDevicedDevice  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gbp_deviced_device_get_info_connect_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;
  g_autoptr(IdeDeviceInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *kernel = NULL;
  g_autofree gchar *system = NULL;
  DevdClient *client;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(client = gbp_deviced_device_get_client_finish (self, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  arch = devd_client_get_arch (client);
  kernel = devd_client_get_kernel (client);
  system = devd_client_get_system (client);

  info = g_object_new (IDE_TYPE_DEVICE_INFO,
                       "arch", arch,
                       "kernel", kernel,
                       "system", system,
                       NULL);

  g_task_return_pointer (task, g_steal_pointer (&info), g_object_unref);
}

static void
gbp_deviced_device_get_info_async (IdeDevice           *device,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)device;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_deviced_device_get_info_async);

  gbp_deviced_device_get_client_async (self,
                                       cancellable,
                                       gbp_deviced_device_get_info_connect_cb,
                                       g_steal_pointer (&task));
}

static IdeDeviceInfo *
gbp_deviced_device_get_info_finish (IdeDevice     *device,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_assert (IDE_IS_DEVICE (device));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gbp_deviced_device_finalize (GObject *object)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;

  IDE_ENTRY;

  g_clear_object (&self->device);
  g_clear_object (&self->client);

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
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->finalize = gbp_deviced_device_finalize;
  object_class->get_property = gbp_deviced_device_get_property;
  object_class->set_property = gbp_deviced_device_set_property;

  device_class->get_info_async = gbp_deviced_device_get_info_async;
  device_class->get_info_finish = gbp_deviced_device_get_info_finish;

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
