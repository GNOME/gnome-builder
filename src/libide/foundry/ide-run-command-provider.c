/* ide-run-command-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-run-command-provider"

#include "config.h"

#include "ide-run-command-provider.h"

G_DEFINE_INTERFACE (IdeRunCommandProvider, ide_run_command_provider, IDE_TYPE_OBJECT)

enum {
  INVALIDATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
ide_run_command_provider_default_init (IdeRunCommandProviderInterface *iface)
{
  signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeRunCommandProviderInterface, invalidated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

void
ide_run_command_provider_list_commands_async (IdeRunCommandProvider *self,
                                              GCancellable          *cancellable,
                                              GAsyncReadyCallback    callback,
                                              gpointer               user_data)
{
  g_return_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUN_COMMAND_PROVIDER_GET_IFACE (self)->list_commands_async (self, cancellable, callback, user_data);
}

/**
 * ide_run_command_provider_list_commands_finish:
 * @self: a #IdeRunCommandProvider
 * @result: a #GAsyncResult
 * @error: location for a #GError
 *
 * Completes request to list run commands.
 *
 * Returns: (transfer full): a #GListModel of #IdeRunCommand
 */
GListModel *
ide_run_command_provider_list_commands_finish (IdeRunCommandProvider  *self,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self), NULL);

  return IDE_RUN_COMMAND_PROVIDER_GET_IFACE (self)->list_commands_finish (self, result, error);
}

/**
 * ide_run_command_provider_invalidate:
 * @self: a #IdeRunCommandProvider
 *
 * Emits the #IdeRunCommandProvider::invalidated signal.
 *
 * This often results in #IdeRunCommands requesting a new set of results for
 * the run command provider via ide_run_command_provider_list_commands_async().
 */
void
ide_run_command_provider_invalidate (IdeRunCommandProvider *self)
{
  g_return_if_fail (IDE_IS_RUN_COMMAND_PROVIDER (self));

  g_signal_emit (self, signals[INVALIDATED], 0);
}
