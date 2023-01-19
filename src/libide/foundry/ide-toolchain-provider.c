/* ide-toolchain-provider.c
 *
 * Copyright 2018 Collabora Ltd.
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-toolchain-provider"

#include "config.h"

#include "ide-marshal.h"

#include "ide-toolchain.h"
#include "ide-toolchain-manager.h"
#include "ide-toolchain-provider.h"

G_DEFINE_INTERFACE (IdeToolchainProvider, ide_toolchain_provider, IDE_TYPE_OBJECT)

enum {
  ADDED,
  REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_toolchain_provider_real_load_async (IdeToolchainProvider *self,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        gpointer              user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_toolchain_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_toolchain_provider_real_load_finish (IdeToolchainProvider  *self,
                                         GAsyncResult          *result,
                                         GError               **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_boolean (G_TASK (self), error);
}

void
ide_toolchain_provider_unload (IdeToolchainProvider *self,
                               IdeToolchainManager  *manager)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (manager));

  IDE_TOOLCHAIN_PROVIDER_GET_IFACE (self)->unload (self, manager);
}

static void
ide_toolchain_provider_real_unload (IdeToolchainProvider *self,
                                    IdeToolchainManager  *manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

}

static void
ide_toolchain_provider_default_init (IdeToolchainProviderInterface *iface)
{
  iface->load_async = ide_toolchain_provider_real_load_async;
  iface->load_finish = ide_toolchain_provider_real_load_finish;
  iface->unload = ide_toolchain_provider_real_unload;

  /**
   * IdeToolchainProvider:added:
   * @self: an #IdeToolchainProvider
   * @toolchain: an #IdeToolchain
   *
   * The "added" signal is emitted when a toolchain
   * has been added to a toolchain provider.
   */
  signals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeToolchainProviderInterface, added),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_TOOLCHAIN);
  g_signal_set_va_marshaller (signals [ADDED],
                              G_TYPE_FROM_INTERFACE (iface),
                              ide_marshal_VOID__OBJECTv);

  /**
   * IdeToolchainProvider:removed:
   * @self: an #IdeToolchainProvider
   * @toolchain: an #IdeToolchain
   *
   * The "removed" signal is emitted when a toolchain
   * has been removed from a toolchain provider.
   */
  signals [REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeToolchainProviderInterface, removed),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_TOOLCHAIN);
  g_signal_set_va_marshaller (signals [REMOVED],
                              G_TYPE_FROM_INTERFACE (iface),
                              ide_marshal_VOID__OBJECTv);

}

/**
 * ide_toolchain_provider_load_async:
 * @self: a #IdeToolchainProvider
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * This function is called to initialize the toolchain provider after
 * the plugin instance has been created. The provider should locate any
 * toolchain within the project and call ide_toolchain_provider_emit_added()
 * before completing the asynchronous function so that the toolchain
 * manager may be made aware of the toolchains.
 */
void
ide_toolchain_provider_load_async (IdeToolchainProvider *self,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));

  IDE_TOOLCHAIN_PROVIDER_GET_IFACE (self)->load_async (self, cancellable, callback, user_data);
}

/**
 * ide_toolchain_provider_load_finish:
 * @self: a #IdeToolchainProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_toolchain_provider_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_toolchain_provider_load_finish (IdeToolchainProvider  *self,
                                    GAsyncResult          *result,
                                    GError               **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_TOOLCHAIN_PROVIDER_GET_IFACE (self)->load_finish (self, result, error);
}

/**
 * ide_toolchain_provider_emit_added:
 * @self: an #IdeToolchainProvider
 * @toolchain: an #IdeToolchain
 *
 * #IdeToolchainProvider implementations should call this function with
 * a @toolchain when it has discovered a new toolchain.
 */
void
ide_toolchain_provider_emit_added (IdeToolchainProvider *self,
                                   IdeToolchain         *toolchain)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN (toolchain));

  g_signal_emit (self, signals [ADDED], 0, toolchain);
}

/**
 * ide_toolchain_provider_emit_removed:
 * @self: an #IdeToolchainProvider
 * @toolchain: an #IdeToolchain
 *
 * #IdeToolchainProvider implementations should call this function with
 * a @toolchain when the toolchain was removed.
 */
void
ide_toolchain_provider_emit_removed (IdeToolchainProvider *self,
                                     IdeToolchain         *toolchain)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN (toolchain));

  g_signal_emit (self, signals [REMOVED], 0, toolchain);
}
