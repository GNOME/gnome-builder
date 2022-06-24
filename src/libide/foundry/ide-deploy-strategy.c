/* ide-deploy-strategy.c
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

#define G_LOG_DOMAIN "ide-deploy-strategy"

#include "config.h"

#include "ide-deploy-strategy.h"
#include "ide-pipeline.h"
#include "ide-run-context.h"
#include "ide-runtime.h"

G_DEFINE_ABSTRACT_TYPE (IdeDeployStrategy, ide_deploy_strategy, IDE_TYPE_OBJECT)

static void
ide_deploy_strategy_real_load_async (IdeDeployStrategy   *self,
                                     IdePipeline         *pipeline,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_deploy_strategy_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not support the current pipeline",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_deploy_strategy_real_load_finish (IdeDeployStrategy  *self,
                                      GAsyncResult       *result,
                                      int                *priority,
                                      GError            **error)
{
  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  *priority = G_MAXINT;

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_deploy_strategy_real_deploy_async (IdeDeployStrategy     *self,
                                       IdePipeline           *pipeline,
                                       GFileProgressCallback  progress,
                                       gpointer               progress_data,
                                       GDestroyNotify         progress_data_destroy,
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_deploy_strategy_real_deploy_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not support the current pipeline",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_deploy_strategy_real_deploy_finish (IdeDeployStrategy  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_deploy_strategy_real_prepare_run_context (IdeDeployStrategy *self,
                                              IdePipeline       *pipeline,
                                              IdeRunContext     *run_context)
{
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  /* In the default implementation, for running locally, we just defer to
   * the pipeline's runtime for how to create a run context.
   */
  if ((runtime = ide_pipeline_get_runtime (pipeline)))
    ide_runtime_prepare_to_run (runtime, pipeline, run_context);
  else
    g_return_if_reached ();

  IDE_EXIT;
}

static void
ide_deploy_strategy_class_init (IdeDeployStrategyClass *klass)
{
  klass->load_async = ide_deploy_strategy_real_load_async;
  klass->load_finish = ide_deploy_strategy_real_load_finish;
  klass->deploy_async = ide_deploy_strategy_real_deploy_async;
  klass->deploy_finish = ide_deploy_strategy_real_deploy_finish;
  klass->prepare_run_context = ide_deploy_strategy_real_prepare_run_context;
}

static void
ide_deploy_strategy_init (IdeDeployStrategy *self)
{
}

/**
 * ide_deploy_strategy_load_async:
 * @self: an #IdeDeployStrategy
 * @pipeline: an #IdePipeline
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the #IdeDeployStrategy load anything
 * necessary to support deployment for @pipeline. If the strategy cannot
 * support the pipeline, it should fail with %G_IO_ERROR error domain
 * and %G_IO_ERROR_NOT_SUPPORTED error code.
 *
 * Generally, the deployment strategy is responsible for checking if
 * it can support deployment to the given device, and determine how to
 * get the install data out of the pipeline. Given so many moving parts
 * in build systems, how to determine that is an implementation detail of
 * the specific #IdeDeployStrategy.
 */
void
ide_deploy_strategy_load_async (IdeDeployStrategy   *self,
                                IdePipeline         *pipeline,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEPLOY_STRATEGY_GET_CLASS (self)->load_async (self, pipeline, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_deploy_strategy_load_finish:
 * @self: an #IdeDeployStrategy
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to load the #IdeDeployStrategy.
 *
 * Returns: %TRUE if successful and the pipeline was supported; otherwise
 *   %FALSE and @error is set.
 */
gboolean
ide_deploy_strategy_load_finish (IdeDeployStrategy  *self,
                                 GAsyncResult       *result,
                                 int                *priority,
                                 GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (priority != NULL);

  ret = IDE_DEPLOY_STRATEGY_GET_CLASS (self)->load_finish (self, result, priority, error);

  IDE_RETURN (ret);
}

/**
 * ide_deploy_strategy_deploy_async:
 * @self: a #IdeDeployStrategy
 * @pipeline: an #IdePipeline
 * @progress: (nullable) (closure progress_data) (scope notified):
 *   a #GFileProgressCallback or %NULL
 * @progress_data: (nullable): closure data for @progress or %NULL
 * @progress_data_destroy: (nullable): destroy callback for @progress_data
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (closure user_data): a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Requests that the #IdeDeployStrategy deploy the application to the
 * configured device in the build pipeline.
 *
 * If supported, the strategy will call @progress with periodic updates as
 * the application is deployed.
 */
void
ide_deploy_strategy_deploy_async (IdeDeployStrategy     *self,
                                  IdePipeline           *pipeline,
                                  GFileProgressCallback  progress,
                                  gpointer               progress_data,
                                  GDestroyNotify         progress_data_destroy,
                                  GCancellable          *cancellable,
                                  GAsyncReadyCallback    callback,
                                  gpointer               user_data)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEPLOY_STRATEGY (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEPLOY_STRATEGY_GET_CLASS (self)->deploy_async (self,
                                                      pipeline,
                                                      progress,
                                                      progress_data,
                                                      progress_data_destroy,
                                                      cancellable,
                                                      callback,
                                                      user_data);

  IDE_EXIT;
}

/**
 * ide_deploy_strategy_deploy_finish:
 * @self: an #IdeDeployStrategy
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to deploy the application to the
 * build pipeline's device.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 */
gboolean
ide_deploy_strategy_deploy_finish (IdeDeployStrategy  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEPLOY_STRATEGY (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = IDE_DEPLOY_STRATEGY_GET_CLASS (self)->deploy_finish (self, result, error);

  IDE_RETURN (ret);
}

/**
 * ide_deploy_strategy_prepare_run_context:
 * @self: a #IdeDeployStrategy
 * @pipeline: an #IdePipeline
 * @run_context: an #IdeRunContext
 *
 * Prepare an #IdeRunContext to run on a device.
 *
 * This virtual function should be implemented by device strategies to prepare
 * a run context for running on a device or deployment situation.
 *
 * Typically this is either nothing (in the case of running locally) or pushing
 * a layer into the run context which is a command to deliver the command to
 * another device/container/simulator/etc.
 */
void
ide_deploy_strategy_prepare_run_context (IdeDeployStrategy *self,
                                         IdePipeline       *pipeline,
                                         IdeRunContext     *run_context)
{
  g_return_if_fail (IDE_IS_DEPLOY_STRATEGY (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));

  IDE_DEPLOY_STRATEGY_GET_CLASS (self)->prepare_run_context (self, pipeline, run_context);
}
