/* ide-command-provider.c
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

#define G_LOG_DOMAIN "ide-command-provider"

#include "config.h"

#include "ide-command-provider.h"

G_DEFINE_INTERFACE (IdeCommandProvider, ide_command_provider, G_TYPE_OBJECT)

static void
ide_command_provider_real_query_async (IdeCommandProvider  *self,
                                       IdeWorkspace        *workspace,
                                       const gchar         *typed_text,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_command_provider_real_query_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Querying is not supported by this provider");
}

static GPtrArray *
ide_command_provider_real_query_finish (IdeCommandProvider  *self,
                                        GAsyncResult        *result,
                                        GError             **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_command_provider_default_init (IdeCommandProviderInterface *iface)
{
  iface->query_async = ide_command_provider_real_query_async;
  iface->query_finish = ide_command_provider_real_query_finish;
}

void
ide_command_provider_query_async (IdeCommandProvider  *self,
                                  IdeWorkspace        *workspace,
                                  const gchar         *typed_text,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_return_if_fail (IDE_IS_COMMAND_PROVIDER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));
  g_return_if_fail (typed_text != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_COMMAND_PROVIDER_GET_IFACE (self)->query_async (self,
                                                      workspace,
                                                      typed_text,
                                                      cancellable,
                                                      callback,
                                                      user_data);
}

/**
 * ide_command_provider_query_finish:
 * @self: a #IdeCommandProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to locate all the commands matching the
 * users typed text.
 *
 * Returns: (transfer full) (element-type IdeCommand): a #GPtrArray of
 *   #IdeCommand, or %NULL.
 *
 * Since: 3.32
 */
GPtrArray *
ide_command_provider_query_finish (IdeCommandProvider  *self,
                                   GAsyncResult        *result,
                                   GError             **error)
{
  g_return_val_if_fail (IDE_IS_COMMAND_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_COMMAND_PROVIDER_GET_IFACE (self)->query_finish (self, result, error);
}

void
ide_command_provider_load_shortcuts (IdeCommandProvider *self,
                                     IdeWorkspace       *workspace)
{
  g_return_if_fail (IDE_IS_COMMAND_PROVIDER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_COMMAND_PROVIDER_GET_IFACE (self)->load_shortcuts)
    IDE_COMMAND_PROVIDER_GET_IFACE (self)->load_shortcuts (self, workspace);
}

void
ide_command_provider_unload_shortcuts (IdeCommandProvider *self,
                                       IdeWorkspace       *workspace)
{
  g_return_if_fail (IDE_IS_COMMAND_PROVIDER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_COMMAND_PROVIDER_GET_IFACE (self)->unload_shortcuts)
    IDE_COMMAND_PROVIDER_GET_IFACE (self)->unload_shortcuts (self, workspace);
}

/**
 * ide_command_provider_get_command_by_id:
 * @self: a #IdeCommandProvider
 * @workspace: an #IdeWorkspace
 * @command_id: the identifier of the command
 *
 * Looks for a command by @command_id and returns it if found.
 *
 * Returns: (transfer full) (nullable): an #IdeCommand or %NULL
 *
 * Since: 3.34
 */
IdeCommand *
ide_command_provider_get_command_by_id (IdeCommandProvider *self,
                                        IdeWorkspace       *workspace,
                                        const gchar        *command_id)
{
  g_return_val_if_fail (IDE_IS_COMMAND_PROVIDER (self), NULL);
  g_return_val_if_fail (IDE_IS_WORKSPACE (workspace), NULL);
  g_return_val_if_fail (command_id != NULL, NULL);

  if (IDE_COMMAND_PROVIDER_GET_IFACE (self)->get_command_by_id)
   return IDE_COMMAND_PROVIDER_GET_IFACE (self)->get_command_by_id (self, workspace, command_id);

  return NULL;
}
