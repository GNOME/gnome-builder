/* ide-local-deploy-strategy.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-local-deploy-strategy"

#include "config.h"

#include <libide-threading.h>

#include "ide-local-deploy-strategy.h"
#include "ide-local-device.h"
#include "ide-pipeline.h"
#include "ide-runtime.h"

struct _IdeLocalDeployStrategy
{
  GObject parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeLocalDeployStrategy, ide_local_deploy_strategy, IDE_TYPE_DEPLOY_STRATEGY)

static void
ide_local_deploy_strategy_load_async (IdeDeployStrategy   *strategy,
                                      IdePipeline         *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (IDE_IS_LOCAL_DEPLOY_STRATEGY (strategy));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (strategy, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_local_deploy_strategy_load_async);

  device = ide_pipeline_get_device (pipeline);

  if (IDE_IS_LOCAL_DEVICE (device))
    ide_task_return_boolean (task, TRUE);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Cannot deploy to this device");

  IDE_EXIT;
}

static gboolean
ide_local_deploy_strategy_load_finish (IdeDeployStrategy  *strategy,
                                       GAsyncResult       *result,
                                       int                *priority,
                                       GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LOCAL_DEPLOY_STRATEGY (strategy));
  g_assert (IDE_IS_TASK (result));
  g_assert (priority != NULL);

  *priority = 0;

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_local_deploy_strategy_deploy_async (IdeDeployStrategy     *strategy,
                                        IdePipeline           *pipeline,
                                        GFileProgressCallback  progress,
                                        gpointer               progress_data,
                                        GDestroyNotify         progress_data_destroy,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeDevice *device;

  IDE_ENTRY;

  g_assert (IDE_IS_LOCAL_DEPLOY_STRATEGY (strategy));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (strategy, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_local_deploy_strategy_deploy_async);

  device = ide_pipeline_get_device (pipeline);

  if (IDE_IS_LOCAL_DEVICE (device))
    ide_task_return_boolean (task, TRUE);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Cannot deploy to this device: %s",
                               device ? G_OBJECT_TYPE_NAME (device) : "None");

  IDE_EXIT;
}

static gboolean
ide_local_deploy_strategy_deploy_finish (IdeDeployStrategy  *strategy,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_LOCAL_DEPLOY_STRATEGY (strategy));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_local_deploy_strategy_class_init (IdeLocalDeployStrategyClass *klass)
{
  IdeDeployStrategyClass *deploy_strategy_class = IDE_DEPLOY_STRATEGY_CLASS (klass);

  deploy_strategy_class->load_async = ide_local_deploy_strategy_load_async;
  deploy_strategy_class->load_finish = ide_local_deploy_strategy_load_finish;
  deploy_strategy_class->deploy_async = ide_local_deploy_strategy_deploy_async;
  deploy_strategy_class->deploy_finish = ide_local_deploy_strategy_deploy_finish;
}

static void
ide_local_deploy_strategy_init (IdeLocalDeployStrategy *self)
{
}

IdeDeployStrategy *
ide_local_deploy_strategy_new (void)
{
  return g_object_new (IDE_TYPE_LOCAL_DEPLOY_STRATEGY, NULL);
}
