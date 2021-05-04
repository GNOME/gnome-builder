/* ide-runtime-provider.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-runtime-provider"

#include "config.h"

#include <libide-threading.h>

#include "ide-pipeline.h"
#include "ide-config.h"
#include "ide-foundry-compat.h"
#include "ide-runtime.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-provider.h"

G_DEFINE_INTERFACE (IdeRuntimeProvider, ide_runtime_provider, IDE_TYPE_OBJECT)

static void
ide_runtime_provider_real_load (IdeRuntimeProvider *self,
                                IdeRuntimeManager  *manager)
{
}

static void
ide_runtime_provider_real_unload (IdeRuntimeProvider *self,
                                  IdeRuntimeManager  *manager)
{
}

static gboolean
ide_runtime_provider_real_provides (IdeRuntimeProvider *self,
                                    const gchar        *runtime_id)
{
  return FALSE;
}

static void
ide_runtime_provider_real_bootstrap_async (IdeRuntimeProvider  *self,
                                           IdePipeline    *pipeline,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_runtime_provider_real_bootstrap_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Bootstrapping runtime not supported for %s",
                             G_OBJECT_TYPE_NAME (self));

  IDE_EXIT;
}

static IdeRuntime *
ide_runtime_provider_real_bootstrap_finish (IdeRuntimeProvider  *self,
                                            GAsyncResult        *result,
                                            GError             **error)
{
  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_runtime_provider_default_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = ide_runtime_provider_real_load;
  iface->unload = ide_runtime_provider_real_unload;
  iface->provides = ide_runtime_provider_real_provides;
  iface->bootstrap_async = ide_runtime_provider_real_bootstrap_async;
  iface->bootstrap_finish = ide_runtime_provider_real_bootstrap_finish;
}

void
ide_runtime_provider_load (IdeRuntimeProvider *self,
                           IdeRuntimeManager  *manager)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (manager));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->load (self, manager);
}

void
ide_runtime_provider_unload (IdeRuntimeProvider *self,
                             IdeRuntimeManager  *manager)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (manager));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->unload (self, manager);
}

gboolean
ide_runtime_provider_provides (IdeRuntimeProvider *self,
                                  const gchar        *runtime_id)
{
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (runtime_id != NULL, FALSE);

  return IDE_RUNTIME_PROVIDER_GET_IFACE (self)->provides (self, runtime_id);
}

/**
 * ide_runtime_provider_bootstrap_async:
 * @self: a #IdeRuntimeProvider
 * @pipeline: an #IdePipeline
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a #GAsyncReadyCallback or %NULL
 * @user_data: closure data for @callback
 *
 * This function allows to the runtime provider to install dependent runtimes
 * similar to ide_runtime_provider_install_async(), but with the added benefit
 * that it can access the pipeline for more information. For example, it may
 * want to check the architecture of the pipeline, or the connected device for
 * tweaks as to what runtime to use.
 *
 * Some runtime providers like Flatpak might use this to locate SDK extensions
 * and install those too.
 *
 * This function should be used instead of ide_runtime_provider_install_async().
 *
 * Since: 3.32
 */
void
ide_runtime_provider_bootstrap_async (IdeRuntimeProvider  *self,
                                      IdePipeline    *pipeline,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->bootstrap_async (self, pipeline, cancellable, callback, user_data);
}

/**
 * ide_runtime_provider_bootstrap_finish:
 * @self: a #IdeRuntimeProvider
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes the asynchronous request to bootstrap.
 *
 * The resulting runtime will be set as the runtime to use for the build
 * pipeline.
 *
 * Returns: (transfer full): an #IdeRuntime if successful; otherwise %NULL
 *   and @error is set.
 *
 * Since: 3.32
 */
IdeRuntime *
ide_runtime_provider_bootstrap_finish (IdeRuntimeProvider  *self,
                                       GAsyncResult        *result,
                                       GError             **error)
{
  IdeRuntime *ret;

  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = IDE_RUNTIME_PROVIDER_GET_IFACE (self)->bootstrap_finish (self, result, error);

  g_return_val_if_fail (!ret || IDE_IS_RUNTIME (ret), NULL);

  return ret;
}
