/* ide-configuration-provider.c
 *
 * Copyright © 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "ide-configuration-provider"

#include "config.h"

#include "application/ide-application.h"
#include "config/ide-configuration.h"
#include "config/ide-configuration-manager.h"
#include "config/ide-configuration-provider.h"

G_DEFINE_INTERFACE (IdeConfigurationProvider, ide_configuration_provider, IDE_TYPE_OBJECT)

enum {
  ADDED,
  REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_configuration_provider_real_load_async (IdeConfigurationProvider *self,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_configuration_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (self));
}

gboolean
ide_configuration_provider_real_load_finish (IdeConfigurationProvider  *self,
                                             GAsyncResult              *result,
                                             GError                   **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_boolean (G_TASK (self), error);
}

static void
ide_configuration_provider_real_duplicate (IdeConfigurationProvider *self,
                                           IdeConfiguration         *config)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_assert (IDE_IS_CONFIGURATION (config));

}

static void
ide_configuration_provider_real_unload (IdeConfigurationProvider *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));

}

void
ide_configuration_provider_real_save_async (IdeConfigurationProvider *self,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_configuration_provider_real_save_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement save_async",
                           G_OBJECT_TYPE_NAME (self));
}

gboolean
ide_configuration_provider_real_save_finish (IdeConfigurationProvider  *self,
                                             GAsyncResult              *result,
                                             GError                   **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_assert (G_IS_TASK (result));
  g_assert (g_task_is_valid (G_TASK (result), self));

  return g_task_propagate_boolean (G_TASK (self), error);
}

static void
ide_configuration_provider_default_init (IdeConfigurationProviderInterface *iface)
{
  iface->load_async = ide_configuration_provider_real_load_async;
  iface->load_finish = ide_configuration_provider_real_load_finish;
  iface->duplicate = ide_configuration_provider_real_duplicate;
  iface->unload = ide_configuration_provider_real_unload;
  iface->save_async = ide_configuration_provider_real_save_async;
  iface->save_finish = ide_configuration_provider_real_save_finish;

  /**
   * IdeConfigurationProvider:added:
   * @self: an #IdeConfigurationProvider
   * @config: an #IdeConfiguration
   *
   * The "added" signal is emitted when a configuration
   * has been added to a configuration provider.
   *
   * Since: 3.28
   */
  signals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeConfigurationProviderInterface, added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_CONFIGURATION);
  g_signal_set_va_marshaller (signals [ADDED],
                              G_TYPE_FROM_INTERFACE (iface),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * IdeConfigurationProvider:removed:
   * @self: an #IdeConfigurationProvider
   * @config: an #IdeConfiguration
   *
   * The "removed" signal is emitted when a configuration
   * has been removed from a configuration provider.
   *
   * Since: 3.28
   */
  signals [REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeConfigurationProviderInterface, removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_CONFIGURATION);
  g_signal_set_va_marshaller (signals [REMOVED],
                              G_TYPE_FROM_INTERFACE (iface),
                              g_cclosure_marshal_VOID__OBJECTv);

}

/**
 * ide_configuration_provider_load_async:
 * @self: a #IdeConfigurationProvider
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * This function is called to initialize the configuration provider after
 * the plugin instance has been created. The provider should locate any
 * build configurations within the project and call
 * ide_configuration_provider_emit_added() before completing the
 * asynchronous function so that the configuration manager may be made
 * aware of the configurations.
 *
 * Since: 3.28
 */
void
ide_configuration_provider_load_async (IdeConfigurationProvider *self,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->load_async (self, cancellable, callback, user_data);
}

/**
 * ide_configuration_provider_load_finish:
 * @self: a #IdeConfigurationProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_configuration_provider_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_configuration_provider_load_finish (IdeConfigurationProvider  *self,
                                        GAsyncResult              *result,
                                        GError                   **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->load_finish (self, result, error);
}

/**
 * ide_configuration_provider_unload:
 * @self: a #IdeConfigurationProvider
 *
 * Requests that the configuration provider unload any state. This is called
 * shortly before the configuration provider is finalized.
 *
 * Implementations of #IdeConfigurationProvider should emit removed
 * for every configuration they have registered so that the
 * #IdeConfigurationManager has correct information.
 */
void
ide_configuration_provider_unload (IdeConfigurationProvider *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->unload (self);
}

/**
 * ide_configuration_provider_save_async:
 * @self: a #IdeConfigurationProvider
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * This function is called to request that the configuration provider
 * persist any changed configurations back to disk.
 *
 * This function will be called before unloading the configuration provider
 * so that it has a chance to persist any outstanding changes.
 */
void
ide_configuration_provider_save_async (IdeConfigurationProvider *self,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->save_async (self, cancellable, callback, user_data);
}

/**
 * ide_configuration_provider_save_finish:
 * @self: a #IdeConfigurationProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_configuration_provider_save_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_configuration_provider_save_finish (IdeConfigurationProvider  *self,
                                        GAsyncResult              *result,
                                        GError                   **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->save_finish (self, result, error);
}

/**
 * ide_configuration_provider_emit_added:
 * @self: an #IdeConfigurationProvider
 * @config: an #IdeConfiguration
 *
 * #IdeConfigurationProvider implementations should call this function with
 * a @config when it has discovered a new configuration.
 *
 * Since: 3.28
 */
void
ide_configuration_provider_emit_added (IdeConfigurationProvider *self,
                                       IdeConfiguration         *config)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  g_signal_emit (self, signals [ADDED], 0, config);
}

/**
 * ide_configuration_provider_emit_removed:
 * @self: an #IdeConfigurationProvider
 * @config: an #IdeConfiguration
 *
 * #IdeConfigurationProvider implementations should call this function with
 * a @config when it has discovered it was removed.
 *
 * Since: 3.28
 */
void
ide_configuration_provider_emit_removed (IdeConfigurationProvider *self,
                                         IdeConfiguration         *config)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  g_signal_emit (self, signals [REMOVED], 0, config);
}

/**
 * ide_configuration_provider_delete:
 * @self: a #IdeConfigurationProvider
 * @config: an #IdeConfiguration owned by the provider
 *
 * Requests that the configuration provider delete the configuration.
 *
 * ide_configuration_provider_save_async() will be called by the
 * #IdeConfigurationManager after calling this function.
 *
 * Since: 3.28
 */
void
ide_configuration_provider_delete (IdeConfigurationProvider *self,
                                   IdeConfiguration         *config)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  if (IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->delete)
    IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->delete (self, config);
  else
    g_warning ("Cannot delete configuration %s",
               ide_configuration_get_id (config));
}

/**
 * ide_configuration_provider_duplicate:
 * @self: an #IdeConfigurationProvider
 * @config: an #IdeConfiguration
 *
 * Requests that the configuration provider duplicate the configuration.
 *
 * This is useful when the user wants to experiment with alternate settings
 * without breaking a previous configuration.
 *
 * The configuration provider does not need to persist the configuration
 * in this function, ide_configuration_provider_save_async() will be called
 * afterwards to persist configurations to disk.
 *
 * It is expected that the #IdeConfigurationProvider will emit
 * #IdeConfigurationProvider::added with the new configuration.
 *
 * Since: 3.28
 */
void
ide_configuration_provider_duplicate (IdeConfigurationProvider *self,
                                      IdeConfiguration         *config)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->duplicate (self, config);
}
