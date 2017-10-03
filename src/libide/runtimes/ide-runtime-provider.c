/* ide-runtime-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "runtimes/ide-runtime-manager.h"
#include "runtimes/ide-runtime-provider.h"

G_DEFINE_INTERFACE (IdeRuntimeProvider, ide_runtime_provider, G_TYPE_OBJECT)

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
ide_runtime_provider_real_can_install (IdeRuntimeProvider *self,
                                       const gchar        *runtime_id)
{
  return FALSE;
}

void
ide_runtime_provider_real_install_async (IdeRuntimeProvider  *self,
                                         const gchar         *runtime_id,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_runtime_provider_real_install_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not support installing runtimes",
                           G_OBJECT_TYPE_NAME (self));
}

gboolean
ide_runtime_provider_real_install_finish (IdeRuntimeProvider  *self,
                                          GAsyncResult        *result,
                                          GError             **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_runtime_provider_default_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = ide_runtime_provider_real_load;
  iface->unload = ide_runtime_provider_real_unload;
  iface->can_install = ide_runtime_provider_real_can_install;
  iface->install_async = ide_runtime_provider_real_install_async;
  iface->install_finish = ide_runtime_provider_real_install_finish;
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
ide_runtime_provider_can_install (IdeRuntimeProvider *self,
                                  const gchar        *runtime_id)
{
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (runtime_id != NULL, FALSE);

  return IDE_RUNTIME_PROVIDER_GET_IFACE (self)->can_install (self, runtime_id);
}

void
ide_runtime_provider_install_async (IdeRuntimeProvider  *self,
                                    const gchar         *runtime_id,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (runtime_id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->install_async (self, runtime_id, cancellable, callback, user_data);
}

gboolean
ide_runtime_provider_install_finish (IdeRuntimeProvider  *self,
                                     GAsyncResult        *result,
                                     GError             **error)
{
  g_return_val_if_fail (IDE_IS_RUNTIME_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_RUNTIME_PROVIDER_GET_IFACE (self)->install_finish (self, result, error);
}
