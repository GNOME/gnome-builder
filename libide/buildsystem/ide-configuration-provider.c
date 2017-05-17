/* ide-configuration-provider.c
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#include "ide-configuration-manager.h"
#include "ide-configuration-provider.h"

G_DEFINE_INTERFACE (IdeConfigurationProvider, ide_configuration_provider, G_TYPE_OBJECT)

static void
ide_configuration_provider_real_load_async (IdeConfigurationProvider *self,
                                            IdeConfigurationManager  *manager,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_autoptr(GTask) task = user_data;

  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (G_IS_TASK (task));

  g_warning ("The current IdeConfigurationProvider doesn't implement load_async");
  g_task_return_boolean (task, TRUE);
}

gboolean
ide_configuration_provider_real_load_finish (IdeConfigurationProvider  *self,
                                             GAsyncResult              *result,
                                             GError                   **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return TRUE;
}

static void
ide_configuration_provider_real_unload (IdeConfigurationProvider *self,
                                        IdeConfigurationManager  *manager)
{
}

void
ide_configuration_provider_real_save_async (IdeConfigurationProvider *self,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  g_autoptr(GTask) task = user_data;

  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (G_IS_TASK (task));

  g_warning ("The current IdeConfigurationProvider doesn't implement save_async");
  g_task_return_boolean (task, TRUE);
}

gboolean
ide_configuration_provider_real_save_finish (IdeConfigurationProvider  *self,
                                             GAsyncResult              *result,
                                             GError                   **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return TRUE;
}

static void
ide_configuration_provider_default_init (IdeConfigurationProviderInterface *iface)
{
  iface->load_async = ide_configuration_provider_real_load_async;
  iface->load_finish = ide_configuration_provider_real_load_finish;
  iface->unload = ide_configuration_provider_real_unload;
  iface->save_async = ide_configuration_provider_real_save_async;
  iface->save_finish = ide_configuration_provider_real_save_finish;
}

void
ide_configuration_provider_load_async (IdeConfigurationProvider *self,
                                       IdeConfigurationManager  *manager,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (manager));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->load_async (self, manager, cancellable, callback, user_data);
}

gboolean
ide_configuration_provider_load_finish (IdeConfigurationProvider  *self,
                                        GAsyncResult              *result,
                                        GError                   **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->save_finish (self, result, error);
}

void
ide_configuration_provider_unload (IdeConfigurationProvider *self,
                                   IdeConfigurationManager  *manager)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONFIGURATION_MANAGER (manager));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->unload (self, manager);
}

void
ide_configuration_provider_save_async (IdeConfigurationProvider *self,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  g_return_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->save_async (self, cancellable, callback, user_data);
}

gboolean
ide_configuration_provider_save_finish (IdeConfigurationProvider  *self,
                                        GAsyncResult              *result,
                                        GError                   **error)
{
  g_return_val_if_fail (IDE_IS_CONFIGURATION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CONFIGURATION_PROVIDER_GET_IFACE (self)->save_finish (self, result, error);
}
