/* gbp-deviced-device.c
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-deviced-device"

#include "gbp-deviced-device.h"

struct _GbpDevicedDevice
{
  IdeDevice      parent_instance;
  DevdDevice    *device;
  DevdClient    *client;
  IdeDeviceInfo *info;
  IdeTask       *connecting_task;
};

typedef struct
{
  gchar                 *local_path;
  gchar                 *remote_path;
  GFileProgressCallback  progress;
  gpointer               progress_data;
  GDestroyNotify         progress_data_destroy;
} InstallBundleState;

G_DEFINE_FINAL_TYPE (GbpDevicedDevice, gbp_deviced_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
install_bundle_state_free (InstallBundleState *state)
{
  g_clear_pointer (&state->local_path, g_free);
  g_clear_pointer (&state->remote_path, g_free);
  if (state->progress_data_destroy)
    state->progress_data_destroy (state->progress_data);
  g_slice_free (InstallBundleState, state);
}

static void
gbp_deviced_device_connect_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpDevicedDevice *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (GBP_IS_DEVICED_DEVICE (self));

  if (task == self->connecting_task)
    g_clear_object (&self->connecting_task);

  if (!devd_client_connect_finish (client, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  g_set_object (&self->client, client);

  ide_task_return_object (task, g_object_ref (client));

  IDE_EXIT;
}

void
gbp_deviced_device_get_client_async (GbpDevicedDevice    *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(DevdClient) client = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_device_get_client_async);

  if (self->client != NULL)
    {
      ide_task_return_object (task, g_object_ref (self->client));
      IDE_EXIT;
    }

  if (self->connecting_task != NULL)
    {
      ide_task_chain (self->connecting_task, task);
      IDE_EXIT;
    }

  g_set_object (&self->connecting_task, task);

  client = devd_device_create_client (self->device);

  ide_task_set_release_on_propagate (task, FALSE);

  devd_client_connect_async (client,
                             cancellable,
                             gbp_deviced_device_connect_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

DevdClient *
gbp_deviced_device_get_client_finish (GbpDevicedDevice  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  DevdClient *client;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (IDE_IS_TASK (result));

  client = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (client);
}

static void
gbp_deviced_device_get_info_connect_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;
  g_autoptr(IdeDeviceInfo) info = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *kernel = NULL;
  g_autofree gchar *system = NULL;
  IdeDeviceKind kind = 0;
  DevdClient *client;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(client = gbp_deviced_device_get_client_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  arch = devd_client_get_arch (client);
  kernel = devd_client_get_kernel (client);
  system = devd_client_get_system (client);
  triplet = ide_triplet_new_with_triplet (arch, kernel, system);

  switch (devd_device_get_kind (self->device))
    {
    case DEVD_DEVICE_KIND_TABLET:
      kind = IDE_DEVICE_KIND_TABLET;
      break;

    case DEVD_DEVICE_KIND_PHONE:
      kind = IDE_DEVICE_KIND_PHONE;
      break;

    case DEVD_DEVICE_KIND_MICRO_CONTROLLER:
      kind = IDE_DEVICE_KIND_MICRO_CONTROLLER;
      break;

    case DEVD_DEVICE_KIND_COMPUTER:
    default:
      kind = IDE_DEVICE_KIND_COMPUTER;
      break;
    }

  info = ide_device_info_new ();
  ide_device_info_set_kind (info, kind);
  ide_device_info_set_host_triplet (info, triplet);

  ide_task_return_object (task, g_steal_pointer (&info));

  IDE_EXIT;
}

static void
gbp_deviced_device_get_info_async (IdeDevice           *device,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)device;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_device_get_info_async);

  gbp_deviced_device_get_client_async (self,
                                       NULL,
                                       gbp_deviced_device_get_info_connect_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

static IdeDeviceInfo *
gbp_deviced_device_get_info_finish (IdeDevice     *device,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  IdeDeviceInfo *info;

  IDE_ENTRY;

  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_TASK (result));

  info = ide_task_propagate_object (IDE_TASK (result), error);

  IDE_RETURN (info);
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
      g_value_set_object (value, gbp_deviced_device_get_device (self));
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
gbp_deviced_device_new (DevdDevice *device)
{
  g_autofree gchar *id = NULL;
  const gchar *name;
  const gchar *icon_name;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (DEVD_IS_DEVICE (device), NULL);

  id = g_strdup_printf ("deviced:%s", devd_device_get_id (device));
  name = devd_device_get_name (device);
  icon_name = devd_device_get_icon_name (device);

  return g_object_new (GBP_TYPE_DEVICED_DEVICE,
                       "id", id,
                       "device", device,
                       "display-name", name,
                       "icon-name", icon_name,
                       NULL);
}

static void
gbp_deviced_device_get_commit_list_apps_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(GPtrArray) apps = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  const gchar *app_id;

  IDE_ENTRY;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(apps = devd_client_list_apps_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  app_id = ide_task_get_task_data (task);

  for (guint i = 0; i < apps->len; i++)
    {
      DevdAppInfo *app_info = g_ptr_array_index (apps, i);

      if (g_strcmp0 (app_id, devd_app_info_get_id (app_info)) == 0)
        {
          const gchar *commit_id = devd_app_info_get_commit_id (app_info);

          if (commit_id != NULL)
            {
              ide_task_return_pointer (task, g_strdup (commit_id), g_free);
              IDE_EXIT;
            }
        }
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_FOUND,
                             "No such application \"%s\"",
                             app_id);

  IDE_EXIT;
}

static void
gbp_deviced_device_get_commit_client_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;
  g_autoptr(DevdClient) client = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(client = gbp_deviced_device_get_client_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  cancellable = ide_task_get_cancellable (task);

  devd_client_list_apps_async (client,
                               cancellable,
                               gbp_deviced_device_get_commit_list_apps_cb,
                               g_steal_pointer (&task));

  IDE_EXIT;
}

void
gbp_deviced_device_get_commit_async (GbpDevicedDevice    *self,
                                     const gchar         *app_id,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_DEVICED_DEVICE (self));
  g_return_if_fail (app_id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_device_get_commit_async);
  ide_task_set_task_data (task, g_strdup (app_id), g_free);

  gbp_deviced_device_get_client_async (self,
                                       cancellable,
                                       gbp_deviced_device_get_commit_client_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

gchar *
gbp_deviced_device_get_commit_finish (GbpDevicedDevice  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  gchar *ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_DEVICED_DEVICE (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
install_bundle_progress (goffset  current_num_bytes,
                         goffset  total_num_bytes,
                         gpointer user_data)
{
  InstallBundleState *state = user_data;

  g_assert (state != NULL);

  if (state->progress && total_num_bytes)
    state->progress (current_num_bytes, total_num_bytes, state->progress_data);
}

static void
install_bundle_install_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  DevdFlatpakService *service = (DevdFlatpakService *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (DEVD_IS_FLATPAK_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!devd_flatpak_service_install_bundle_finish (service, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  /* TODO: Remove bundle */

  IDE_EXIT;

}

static void
install_bundle_put_file_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  DevdTransferService *service = (DevdTransferService *)object;
  g_autoptr(DevdFlatpakService) flatpak = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  InstallBundleState *state;
  DevdClient *client;

  IDE_ENTRY;

  g_assert (DEVD_IS_TRANSFER_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!devd_transfer_service_put_file_finish (service, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  client = devd_service_get_client (DEVD_SERVICE (service));

  if (!(flatpak = devd_flatpak_service_new (client, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->remote_path != NULL);

  devd_flatpak_service_install_bundle_async (flatpak,
                                             state->remote_path,
                                             ide_task_get_cancellable (task),
                                             install_bundle_install_cb,
                                             g_object_ref (task));

  IDE_EXIT;
}

static void
install_bundle_get_client_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GbpDevicedDevice *self = (GbpDevicedDevice *)object;
  g_autoptr(DevdTransferService) service = NULL;
  g_autoptr(DevdClient) client = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GFile) file = NULL;
  InstallBundleState *state;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->local_path != NULL);

  if (!(client = gbp_deviced_device_get_client_finish (self, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!(service = devd_transfer_service_new (client, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  file = g_file_new_for_path (state->local_path);

  devd_transfer_service_put_file_async (service,
                                        file,
                                        state->remote_path,
                                        install_bundle_progress, state, NULL,
                                        ide_task_get_cancellable (task),
                                        install_bundle_put_file_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}

void
gbp_deviced_device_install_bundle_async (GbpDevicedDevice      *self,
                                         const gchar           *bundle_path,
                                         GFileProgressCallback  progress,
                                         gpointer               progress_data,
                                         GDestroyNotify         progress_data_destroy,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *name = NULL;
  InstallBundleState *state;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_DEVICED_DEVICE (self));
  g_return_if_fail (bundle_path != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_device_install_bundle_async);

  name = g_path_get_basename (bundle_path);

  state = g_slice_new0 (InstallBundleState);
  state->local_path = g_strdup (bundle_path);
  state->remote_path = g_build_filename (".cache", "deviced", name, NULL);
  state->progress = progress;
  state->progress_data = progress_data;
  state->progress_data_destroy = progress_data_destroy;
  ide_task_set_task_data (task, state, install_bundle_state_free);

  gbp_deviced_device_get_client_async (self,
                                       cancellable,
                                       install_bundle_get_client_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_deviced_device_install_bundle_finish (GbpDevicedDevice  *self,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_DEVICED_DEVICE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

DevdDevice *
gbp_deviced_device_get_device (GbpDevicedDevice *self)
{
  g_return_val_if_fail (GBP_IS_DEVICED_DEVICE (self), NULL);

  return self->device;
}
