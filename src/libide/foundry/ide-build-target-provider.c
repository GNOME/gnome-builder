/* ide-build-target-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-target-provider"

#include "config.h"

#include "ide-build-target-provider.h"

G_DEFINE_INTERFACE (IdeBuildTargetProvider, ide_build_target_provider, G_TYPE_OBJECT)

static void
ide_build_target_provider_real_get_targets_async (IdeBuildTargetProvider *provider,
                                                  GCancellable           *cancellable,
                                                  GAsyncReadyCallback     callback,
                                                  gpointer                user_data)
{
  g_task_report_new_error (provider, callback, user_data,
                           ide_build_target_provider_real_get_targets_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Loading targets is not supported by %s",
                           G_OBJECT_TYPE_NAME (provider));
}

static GPtrArray *
ide_build_target_provider_real_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                   GAsyncResult            *result,
                                                   GError                 **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_build_target_provider_default_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = ide_build_target_provider_real_get_targets_async;
  iface->get_targets_finish = ide_build_target_provider_real_get_targets_finish;
}

/**
 * ide_build_target_provider_get_targets_async:
 * @self: an #IdeBuildTargetProvider
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (scope async): a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the provider fetch all of the known build
 * targets that are part of the project. Generally this should be limited to
 * executables that Builder might be interested in potentially running.
 *
 * @callback should call ide_build_target_provider_get_targets_finish() to
 * complete the asynchronous operation.
 *
 * See also: ide_build_target_provider_get_targets_finish()
 */
void
ide_build_target_provider_get_targets_async (IdeBuildTargetProvider *self,
                                             GCancellable           *cancellable,
                                             GAsyncReadyCallback     callback,
                                             gpointer                user_data)
{
  g_return_if_fail (IDE_IS_BUILD_TARGET_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_TARGET_PROVIDER_GET_IFACE (self)->get_targets_async (self,
                                                                 cancellable,
                                                                 callback,
                                                                 user_data);
}

/**
 * ide_build_target_provider_get_targets_finish:
 * @self: an #IdeBuildTargetProvider
 * @result: a #GAsyncResult provided to the callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to get the targets for the project.
 *
 * See also: ide_build_target_provider_get_targets_async()
 *
 * Returns: (transfer full) (element-type Ide.BuildTarget): The array of
 *   build targets or %NULL upon failure and @error is set.
 */
GPtrArray *
ide_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *self,
                                              GAsyncResult            *result,
                                              GError                 **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_TARGET_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_BUILD_TARGET_PROVIDER_GET_IFACE (self)->get_targets_finish (self, result, error);
}
