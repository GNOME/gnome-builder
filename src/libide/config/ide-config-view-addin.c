/* ide-config-view-addin.c
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

#include "config.h"

#define G_LOG_DOMAIN "ide-config-view-addin"

#include "config/ide-config-view-addin.h"

/**
 * SECTION:ide-config-view-addin
 * @title: IdeConfigViewAddin
 * @short_description: addins for extending the configuration view
 *
 * The #IdeConfigViewAddin allows plugins to add widgets for configuring
 * a build configuration.
 *
 * Since: 3.32
 */

G_DEFINE_INTERFACE (IdeConfigViewAddin, ide_config_view_addin, IDE_TYPE_OBJECT)

static void
ide_config_view_addin_real_load_async (IdeConfigViewAddin  *self,
                                       DzlPreferences      *preferences,
                                       IdeConfiguration    *configuration,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_config_view_addin_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Load async not implemented");
}

static gboolean
ide_config_view_addin_real_load_finish (IdeConfigViewAddin  *self,
                                        GAsyncResult        *result,
                                        GError             **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_config_view_addin_default_init (IdeConfigViewAddinInterface *iface)
{
  iface->load_async = ide_config_view_addin_real_load_async;
  iface->load_finish = ide_config_view_addin_real_load_finish;
}

/**
 * ide_config_view_addin_load_async:
 * @self: an #IdeConfigViewAddin
 * @preferences: a #DzlPreferences
 * @config: an #IdeConfiguration
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously loads any preferences that are part of the plugin
 * in relation to @config.
 *
 * Since: 3.32
 */
void
ide_config_view_addin_load_async (IdeConfigViewAddin  *self,
                                  DzlPreferences      *preferences,
                                  IdeConfiguration    *config,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_CONFIG_VIEW_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->load_async (self, preferences, config,
                                                      cancellable, callback, user_data);
}

/**
 * ide_config_view_addin_load_finish:
 * @self: an #IdeConfigViewAddin
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_config_view_addin_load_finish (IdeConfigViewAddin  *self,
                                   GAsyncResult        *result,
                                   GError             **error)
{
  g_return_val_if_fail (IDE_IS_CONFIG_VIEW_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->load_finish (self, result, error);
}

/**
 * ide_config_view_addin_unload:
 * @self: an #IdeConfigViewAddin
 * @preferences: a #DzlPreferences
 * @config: an #IdeConfiguration
 *
 * This is called when the plugin should release any of it's previously
 * registered settings in ide_config_view_addin_load_async(). This can happen
 * when the plugin is unloaded or @preferences is being destroyed.
 *
 * Since: 3.32
 */
void
ide_config_view_addin_unload (IdeConfigViewAddin *self,
                              DzlPreferences     *preferences,
                              IdeConfiguration   *config)
{
  g_return_if_fail (IDE_IS_CONFIG_VIEW_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));
  g_return_if_fail (IDE_IS_CONFIGURATION (config));

  if (IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->unload)
    IDE_CONFIG_VIEW_ADDIN_GET_IFACE (self)->unload (self, preferences, config);
}
