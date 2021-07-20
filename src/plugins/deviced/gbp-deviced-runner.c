/* gbp-deviced-runner.c
 *
 * Copyright 2021 James Westman <james@jwestman.net>
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

#define G_LOG_DOMAIN "gbp-deviced-runner"

#include "../flatpak/gbp-flatpak-manifest.h"

#include "gbp-deviced-runner.h"

struct _GbpDevicedRunner
{
  IdeRunner parent_instance;

  GbpDevicedDevice *device;
};

G_DEFINE_TYPE (GbpDevicedRunner, gbp_deviced_runner, IDE_TYPE_RUNNER)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpDevicedRunner *
gbp_deviced_runner_new (GbpDevicedDevice *device)
{
  return g_object_new (GBP_TYPE_DEVICED_RUNNER,
                       "device", device,
                       NULL);
}

static void
gbp_deviced_runner_finalize (GObject *object)
{
  GbpDevicedRunner *self = (GbpDevicedRunner *)object;

  g_clear_object (&self->device);

  G_OBJECT_CLASS (gbp_deviced_runner_parent_class)->finalize (object);
}

static void
gbp_deviced_runner_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpDevicedRunner *self = GBP_DEVICED_RUNNER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, gbp_deviced_runner_get_device (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_deviced_runner_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpDevicedRunner *self = GBP_DEVICED_RUNNER (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      gbp_deviced_runner_set_device (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

typedef struct {
  GbpDevicedRunner *self;
  DevdClient *client;
  char *app_id;
  DevdProcessService *process_service;
  char *process_id;
  char *pty_id;
  int tty_fd;
  GCancellable *cancellable;
  gulong cancellable_handle;
} RunData;

static void
run_data_free (RunData *data)
{
  g_clear_object (&data->self);
  g_clear_object (&data->client);
  g_clear_object (&data->process_service);
  g_clear_pointer (&data->app_id, g_free);
  g_clear_pointer (&data->process_id, g_free);
  g_clear_pointer (&data->pty_id, g_free);
  g_cancellable_disconnect (data->cancellable, data->cancellable_handle);
  g_free (data);
}

static void run_wait_for_process_loop (IdeTask *task);

static void
run_wait_for_process_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  DevdProcessService *process_service = (DevdProcessService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  RunData *data;
  gboolean exited;
  int exit_code;
  int term_sig;
  GCancellable *cancellable;

  g_assert (DEVD_IS_PROCESS_SERVICE (process_service));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  devd_process_service_wait_for_process_finish (process_service,
                                                result,
                                                &exited,
                                                &exit_code,
                                                &term_sig,
                                                &error);
  if (error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (exited)
    {
      if (data->pty_id != NULL)
        devd_process_service_destroy_pty_async (process_service,
                                                data->pty_id,
                                                cancellable,
                                                NULL, NULL);

      if (data->tty_fd != -1)
        close(data->tty_fd);

      ide_task_return_boolean (task, TRUE);
    }
  else
    {
      run_wait_for_process_loop (task);
    }
}

static void
run_wait_for_process_loop (IdeTask *task)
{
  GCancellable *cancellable;
  RunData *data;

  data = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  devd_process_service_wait_for_process_async (data->process_service,
                                               data->process_id,
                                               cancellable,
                                               run_wait_for_process_cb,
                                               g_object_ref (task));
}

static void
run_cancelled_cb (GCancellable *cancellable,
                  RunData      *data)
{
  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));

  if (data->process_service)
    devd_process_service_force_exit (data->process_service, data->process_id);

  IDE_EXIT;
}

static void
run_run_app_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  RunData *data;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);

  if (!(data->process_id = devd_client_run_app_finish (client, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data->cancellable = ide_task_get_cancellable (task);
  g_cancellable_connect (data->cancellable, G_CALLBACK (run_cancelled_cb), data, NULL);

  run_wait_for_process_loop (task);
}

static void
run_create_pty_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  RunData *data;
  DevdProcessService *process_service = (DevdProcessService *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (DEVD_IS_PROCESS_SERVICE (process_service));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);

  if (!(data->pty_id = devd_process_service_create_pty_finish (process_service, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  devd_client_run_app_async (data->client,
                             "flatpak",
                             data->app_id,
                             data->pty_id,
                             ide_task_get_cancellable (task),
                             run_run_app_cb,
                             g_object_ref (task));

  IDE_EXIT;
}

static void
run_get_client_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  RunData *data;
  GbpDevicedDevice *device = (GbpDevicedDevice *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  VtePty *pty;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (device));
  g_assert (IDE_IS_TASK (task));

  data = ide_task_get_task_data (task);

  if (!(data->client = gbp_deviced_device_get_client_finish (device, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  pty = ide_runner_get_pty (IDE_RUNNER (data->self));
  if (pty == NULL || ide_runner_get_disable_pty (IDE_RUNNER (data->self)))
    {
      devd_client_run_app_async (data->client,
                                 "flatpak",
                                 data->app_id,
                                 NULL,
                                 ide_task_get_cancellable (task),
                                 run_run_app_cb,
                                 g_object_ref (task));
      IDE_EXIT;
    }

  data->process_service = devd_process_service_new (data->client, &error);
  if (error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  data->tty_fd = ide_pty_intercept_create_slave (vte_pty_get_fd (pty), TRUE);
  devd_process_service_create_pty_async (data->process_service,
                                         data->tty_fd,
                                         ide_task_get_cancellable (task),
                                         run_create_pty_cb,
                                         g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_deviced_runner_run_async (IdeRunner           *runner,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GbpDevicedRunner *self = (GbpDevicedRunner *)runner;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;
  IdeConfigManager *config_manager;
  IdeConfig *config;
  RunData *data;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_DEVICED_RUNNER (self));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_runner_run_async);

  if (self->device == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No device set on deviced runner");
      IDE_EXIT;
    }

  context = ide_object_get_context (IDE_OBJECT (self->device));
  g_assert (IDE_IS_CONTEXT (context));
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  data = g_new0 (RunData, 1);

  /* GbpDevicedDeployStrategy only supports flatpak manifests, and it is the
   * only thing that creates GbpDevicedRunners, so we should only get flatpak
   * manifests here */
  g_assert (GBP_IS_FLATPAK_MANIFEST (config));

  ide_task_set_task_data (task, data, run_data_free);

  data->self = g_object_ref (self);
  data->app_id = g_strdup (ide_config_get_app_id (config));
  data->tty_fd = -1;

  gbp_deviced_device_get_client_async (self->device,
                                       cancellable,
                                       run_get_client_cb,
                                       g_object_ref (task));

  IDE_EXIT;
}

static gboolean
gbp_deviced_runner_run_finish (IdeRunner         *self,
                               GAsyncResult      *result,
                               GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_DEVICED_RUNNER (self), FALSE);
  g_return_val_if_fail (ide_task_is_valid (result, self), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_deviced_runner_class_init (GbpDevicedRunnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRunnerClass *runner_class = IDE_RUNNER_CLASS (klass);

  object_class->finalize = gbp_deviced_runner_finalize;
  object_class->get_property = gbp_deviced_runner_get_property;
  object_class->set_property = gbp_deviced_runner_set_property;

  runner_class->run_async = gbp_deviced_runner_run_async;
  runner_class->run_finish = gbp_deviced_runner_run_finish;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device to run on",
                         GBP_TYPE_DEVICED_DEVICE,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (klass), N_PROPS, properties);
}

static void
gbp_deviced_runner_init (GbpDevicedRunner *self)
{
}

GbpDevicedDevice *
gbp_deviced_runner_get_device (GbpDevicedRunner *self)
{
  g_return_val_if_fail (GBP_IS_DEVICED_RUNNER (self), NULL);
  return self->device;
}

void
gbp_deviced_runner_set_device (GbpDevicedRunner *self,
                               GbpDevicedDevice *device)
{
  g_return_if_fail (GBP_IS_DEVICED_RUNNER (self));
  g_set_object (&self->device, device);
}
