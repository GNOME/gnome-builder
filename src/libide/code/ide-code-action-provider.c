/* ide-code-action-provider.c
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#define G_LOG_DOMAIN "ide-code-action-provider"

#include "config.h"

#include "ide-buffer.h"
#include "ide-code-action-provider.h"

G_DEFINE_INTERFACE (IdeCodeActionProvider, ide_code_action_provider, G_TYPE_OBJECT)

static void
ide_code_action_provider_real_query_async (IdeCodeActionProvider *self,
                                           IdeBuffer             *buffer,
                                           GCancellable          *cancellable,
                                           GAsyncReadyCallback    callback,
                                           gpointer               user_data)
{
  g_assert (IDE_IS_CODE_ACTION_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_code_action_provider_real_query_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "The operation is not supported");
}

static GPtrArray*
ide_code_action_provider_real_query_finish (IdeCodeActionProvider  *self,
                                            GAsyncResult           *result,
                                            GError                **error)
{
  g_assert (IDE_IS_CODE_ACTION_PROVIDER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_code_action_provider_real_set_diagnostics (IdeCodeActionProvider  *self,
                                               IdeDiagnostics         *diags)
{
  g_assert (IDE_IS_CODE_ACTION_PROVIDER (self));
  g_assert (!diags || IDE_IS_DIAGNOSTICS (diags));
}

static void
ide_code_action_provider_default_init (IdeCodeActionProviderInterface *iface)
{
  iface->query_async = ide_code_action_provider_real_query_async;
  iface->query_finish = ide_code_action_provider_real_query_finish;
  iface->set_diagnostics = ide_code_action_provider_real_set_diagnostics;
}

void
ide_code_action_provider_query_async (IdeCodeActionProvider *self,
                                      IdeBuffer             *buffer,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      gpointer               user_data)
{
  g_return_if_fail (IDE_IS_CODE_ACTION_PROVIDER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CODE_ACTION_PROVIDER_GET_IFACE (self)->query_async (self,
                                                          buffer,
                                                          cancellable,
                                                          callback,
                                                          user_data);
}

/**
 * ide_code_action_provider_query_finish:
 * @self: an #IdeBuffer
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_code_action_provider_query_async().
 *
 * Returns: (transfer full) (element-type IdeCodeAction): a #GPtrArray of #IdeCodeAction.
 */
GPtrArray*
ide_code_action_provider_query_finish (IdeCodeActionProvider  *self,
                                       GAsyncResult           *result,
                                       GError                **error)
{
  g_return_val_if_fail (IDE_IS_CODE_ACTION_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CODE_ACTION_PROVIDER_GET_IFACE (self)->query_finish (self, result, error);
}

void
ide_code_action_provider_load (IdeCodeActionProvider *self)
{
  g_return_if_fail (IDE_IS_CODE_ACTION_PROVIDER (self));

  if (IDE_CODE_ACTION_PROVIDER_GET_IFACE (self)->load)
    IDE_CODE_ACTION_PROVIDER_GET_IFACE (self)->load (self);
}

void
ide_code_action_provider_set_diagnostics (IdeCodeActionProvider *self,
                                          IdeDiagnostics        *diags)
{
  g_return_if_fail (IDE_IS_CODE_ACTION_PROVIDER (self));

  IDE_CODE_ACTION_PROVIDER_GET_IFACE (self)->set_diagnostics (self, diags);
}
