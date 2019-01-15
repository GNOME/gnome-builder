/* gbp-deviced-deploy-strategy.c
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

#define G_LOG_DOMAIN "gbp-deviced-deploy-strategy"

#include "../flatpak/gbp-flatpak-manifest.h"
#include "../flatpak/gbp-flatpak-util.h"

#include "gbp-deviced-deploy-strategy.h"
#include "gbp-deviced-device.h"

struct _GbpDevicedDeployStrategy
{
  IdeObject parent_instance;
};

typedef struct
{
  IdePipeline      *pipeline;
  GbpDevicedDevice      *device;
  gchar                 *app_id;
  gchar                 *flatpak_path;
  GFileProgressCallback  progress;
  gpointer               progress_data;
  GDestroyNotify         progress_data_destroy;
} DeployState;

G_DEFINE_TYPE (GbpDevicedDeployStrategy, gbp_deviced_deploy_strategy, IDE_TYPE_DEPLOY_STRATEGY)

static void
deploy_state_free (DeployState *state)
{
  g_clear_object (&state->pipeline);
  g_clear_object (&state->device);
  g_clear_pointer (&state->app_id, g_free);
  g_clear_pointer (&state->flatpak_path, g_free);
  if (state->progress_data_destroy)
    state->progress_data_destroy (state->progress_data);
  g_slice_free (DeployState, state);
}

static void
gbp_deviced_deploy_strategy_load_async (IdeDeployStrategy   *strategy,
                                        IdePipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevicedDeployStrategy *self = (GbpDevicedDeployStrategy *)strategy;
  g_autoptr(IdeTask) task = NULL;
  IdeConfig *config;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_deploy_strategy_load_async);

  config = ide_pipeline_get_config (pipeline);

  if (!(device = ide_pipeline_get_device (pipeline)) ||
      !GBP_IS_DEVICED_DEVICE (device) ||
      !GBP_IS_FLATPAK_MANIFEST (config))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "%s is not supported by %s",
                                 device ?  G_OBJECT_TYPE_NAME (device) : "(nil)",
                                 G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
deploy_install_bundle_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GbpDevicedDevice *device = (GbpDevicedDevice *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEVICE (device));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_deviced_device_install_bundle_finish (device, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
deploy_progress_cb (goffset  current_num_bytes,
                    goffset  total_num_bytes,
                    gpointer user_data)
{
  DeployState *state = user_data;

  g_assert (state != NULL);

  if (state->progress && total_num_bytes)
    state->progress (current_num_bytes, total_num_bytes, state->progress_data);
}

static void
deploy_wait_check_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  DeployState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    gbp_deviced_device_install_bundle_async (state->device,
                                             state->flatpak_path,
                                             deploy_progress_cb, state, NULL,
                                             ide_task_get_cancellable (task),
                                             deploy_install_bundle_cb,
                                             g_object_ref (task));

  IDE_EXIT;
}

static void
deploy_get_commit_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GbpDevicedDevice *device = (GbpDevicedDevice *)object;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autofree gchar *commit_id = NULL;
  g_autofree gchar *dest_path = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *repo_dir = NULL;
  g_autofree gchar *staging_dir = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  IdeConfig *config;
  IdeToolchain *toolchain;
  const gchar *arch;
  const gchar *app_id;
  DeployState *state;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_DEVICED_DEVICE (device));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (IDE_IS_PIPELINE (state->pipeline));

  commit_id = gbp_deviced_device_get_commit_finish (device, result, NULL);

  context = ide_object_get_context (IDE_OBJECT (state->pipeline));
  config = ide_pipeline_get_config (state->pipeline);
  toolchain = ide_pipeline_get_toolchain (state->pipeline);
  triplet = ide_toolchain_get_host_triplet (toolchain);
  arch = ide_triplet_get_arch (triplet);
  staging_dir = gbp_flatpak_get_staging_dir (state->pipeline);
  repo_dir = gbp_flatpak_get_repo_dir (context);
  app_id = ide_config_get_app_id (config);
#if 0
  if (commit_id != NULL)
    name = g_strdup_printf ("%s-%s.flatpak", app_id, commit_id);
  else
#endif
  name = g_strdup_printf ("%s.flatpak", app_id);
  dest_path = g_build_filename (staging_dir, name, NULL);

  state->flatpak_path = g_strdup (dest_path);

  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_push_argv (launcher, "flatpak");
  ide_subprocess_launcher_push_argv (launcher, "build-bundle");
  ide_subprocess_launcher_push_argv (launcher, "-vv");
  ide_subprocess_launcher_push_argv (launcher, "--arch");
  ide_subprocess_launcher_push_argv (launcher, arch ?: ide_get_system_arch ());
  ide_subprocess_launcher_push_argv (launcher, repo_dir);
  ide_subprocess_launcher_push_argv (launcher, dest_path);
  ide_subprocess_launcher_push_argv (launcher, app_id);
  /* TODO: pretend to be master until config has way to set branch */
  ide_subprocess_launcher_push_argv (launcher, "master");

#if 0
  if (commit_id != NULL)
    {
      /*
       * If we got a commit_id, we can build a static delta between the
       * two versions (if we have it). Otherwise, we'll just deploy a full
       * image of the entire app contents.
       */
      IDE_TRACE_MSG ("Building static delta from commit %s", commit_id);
      ide_subprocess_launcher_push_argv (launcher, "--from-commit");
      ide_subprocess_launcher_push_argv (launcher, commit_id);
    }
#endif

  ide_pipeline_attach_pty (state->pipeline, launcher);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_subprocess_wait_check_async (subprocess,
                                     ide_task_get_cancellable (task),
                                     deploy_wait_check_cb,
                                     g_object_ref (task));

  IDE_EXIT;
}

static void
deploy_commit_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdePipeline *pipeline = (IdePipeline *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GCancellable *cancellable;
  DeployState *state;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  /*
   * If we successfully exported the build to a repo, we can now check
   * what version we have on the other side. We might be able to save
   * some data transfer by building a static-delta.
   */

  cancellable = ide_task_get_cancellable (task);
  state = ide_task_get_task_data (task);

  if (!ide_pipeline_build_finish (pipeline, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    gbp_deviced_device_get_commit_async (state->device,
                                         state->app_id,
                                         cancellable,
                                         deploy_get_commit_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_deviced_deploy_strategy_deploy_async (IdeDeployStrategy     *strategy,
                                          IdePipeline      *pipeline,
                                          GFileProgressCallback  progress,
                                          gpointer               progress_data,
                                          GDestroyNotify         progress_data_destroy,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  GbpDevicedDeployStrategy *self = (GbpDevicedDeployStrategy *)strategy;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeTriplet) triplet = NULL;
  IdeConfig *config;
  DeployState *state;
  const gchar *app_id;
  const gchar *arch;
  IdeDevice *device;
  IdeToolchain *toolchain;

  IDE_ENTRY;

  g_assert (GBP_IS_DEVICED_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_deviced_deploy_strategy_deploy_async);

  config = ide_pipeline_get_config (pipeline);
  device = ide_pipeline_get_device (pipeline);
  toolchain = ide_pipeline_get_toolchain (pipeline);
  triplet = ide_toolchain_get_host_triplet (toolchain);
  arch = ide_triplet_get_arch (triplet);
  app_id = ide_config_get_app_id (config);

  g_assert (GBP_IS_FLATPAK_MANIFEST (config));
  g_assert (GBP_IS_DEVICED_DEVICE (device));

  state = g_slice_new (DeployState);
  state->pipeline = g_object_ref (pipeline);
  state->app_id = g_strdup_printf ("%s/%s/master", app_id, arch);
  state->device = g_object_ref (GBP_DEVICED_DEVICE (device));
  state->progress = progress;
  state->progress_data = progress_data;
  state->progress_data_destroy = progress_data_destroy;
  ide_task_set_task_data (task, state, deploy_state_free);

  /*
   * First make sure we've built up to the point where we have a
   * build-finish/build-export in the flatpak plugin.
   */

  ide_pipeline_build_async (pipeline,
                                  IDE_PIPELINE_PHASE_COMMIT,
                                  cancellable,
                                  deploy_commit_cb,
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
