/* gbp-deviced-deploy-strategy.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-deviced-deploy-strategy"

#include "../flatpak/gbp-flatpak-manifest.h"

#include "gbp-deviced-deploy-strategy.h"
#include "gbp-deviced-device.h"

struct _GbpDevicedDeployStrategy
{
  IdeObject parent_instance;
};

typedef struct
{
  IdeBuildPipeline *pipeline;
  gchar            *app_id;
} DeployState;

G_DEFINE_TYPE (GbpDevicedDeployStrategy, gbp_deviced_deploy_strategy, IDE_TYPE_DEPLOY_STRATEGY)

static void
deploy_state_free (DeployState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_pointer (&state->app_id, g_free);
  g_slice_free (DeployState, state);
}

static void
gbp_deviced_deploy_strategy_load_async (IdeDeployStrategy   *strategy,
                                        IdeBuildPipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevicedDeployStrategy *self = (GbpDevicedDeployStrategy *)strategy;
  g_autoptr(GTask) task = NULL;
  IdeConfiguration *config;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_deviced_deploy_strategy_load_async);

  config = ide_build_pipeline_get_configuration (pipeline);

  if (!(device = ide_build_pipeline_get_device (pipeline)) ||
      !GBP_IS_DEVICED_DEVICE (device) ||
      !GBP_IS_FLATPAK_MANIFEST (config))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "%s is not supported by %s",
                               G_OBJECT_TYPE_NAME (device),
                               G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
deploy_get_commit_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GbpDevicedDevice *device = (GbpDevicedDevice *)object;
  g_autoptr(GTask) task = user_data;
  g_autofree gchar *commit_id = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (device));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  commit_id = gbp_deviced_device_get_commit_finish (device, result, NULL);

  /*
   * If we got a commit_id, we can build a static delta between the
   * two versions (if we have it). Otherwise, we'll just deploy a full
   * image of the entire app contents.
   */

  /* TODO */

  g_print ("Commit id is %s\n", commit_id);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_deviced_deploy_strategy_deploy_async (IdeDeployStrategy     *strategy,
                                          IdeBuildPipeline      *pipeline,
                                          GFileProgressCallback  progress,
                                          gpointer               process_data,
                                          GDestroyNotify         progress_data_destroy,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  GbpDevicedDeployStrategy *self = (GbpDevicedDeployStrategy *)strategy;
  g_autoptr(GTask) task = NULL;
  IdeConfiguration *config;
  DeployState *state;
  const gchar *app_id;
  const gchar *arch;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_deviced_deploy_strategy_deploy_async);

  config = ide_build_pipeline_get_configuration (pipeline);
  device = ide_build_pipeline_get_device (pipeline);
  arch = ide_build_pipeline_get_arch (pipeline);
  app_id = ide_configuration_get_app_id (config);

  g_assert (GBP_IS_FLATPAK_MANIFEST (config));
  g_assert (GBP_IS_DEVICED_DEVICE (device));

  state = g_slice_new (DeployState);
  state->pipeline = g_object_ref (pipeline);
  state->app_id = g_strdup_printf ("%s/%s/master", app_id, arch);
  g_task_set_task_data (task, state, (GDestroyNotify)deploy_state_free);

  /*
   * First, we want to check to see what version of the application the
   * device already has. If it is already installed, we can generate a
   * static-delta instead of copying the entire app to the device.
   */

  gbp_deviced_device_get_commit_async (GBP_DEVICED_DEVICE (device),
                                       state->app_id,
                                       cancellable,
                                       deploy_get_commit_cb,
                                       g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_deviced_deploy_strategy_class_init (GbpDevicedDeployStrategyClass *klass)
{
  IdeDeployStrategyClass *strategy_class = IDE_DEPLOY_STRATEGY_CLASS (klass);

  strategy_class->load_async = gbp_deviced_deploy_strategy_load_async;
  strategy_class->deploy_async = gbp_deviced_deploy_strategy_deploy_async;
}

static void
gbp_deviced_deploy_strategy_init (GbpDevicedDeployStrategy *self)
{
}
