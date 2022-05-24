/* ide-rename-provider.c
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

#define G_LOG_DOMAIN "ide-rename-provider"

#include "config.h"

#include <libide-threading.h>

#include "ide-rename-provider.h"

G_DEFINE_INTERFACE (IdeRenameProvider, ide_rename_provider, IDE_TYPE_OBJECT)

static void
ide_rename_provider_real_rename_async (IdeRenameProvider   *self,
                                       IdeLocation   *location,
                                       const gchar         *new_name,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_RENAME_PROVIDER (self));
  g_assert (location != NULL);
  g_assert (new_name != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_rename_provider_real_rename_async);

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "%s has not implemented rename_async",
                             G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_rename_provider_real_rename_finish (IdeRenameProvider  *self,
                                        GAsyncResult       *result,
                                        GPtrArray         **edits,
                                        GError            **error)
{
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_rename_provider_default_init (IdeRenameProviderInterface *iface)
{
  iface->rename_async = ide_rename_provider_real_rename_async;
  iface->rename_finish = ide_rename_provider_real_rename_finish;
}

/**
 * ide_rename_provider_rename_async:
 * @self: An #IdeRenameProvider
 * @location: An #IdeLocation
 * @new_name: The replacement name for the symbol
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to complete the request
 * @user_data: user data for @callback
 *
 * This requests the provider to determine the edits that must be made to the
 * project to perform the renaming of a symbol found at @location.
 *
 * Use ide_rename_provider_rename_finish() to get the results.
 */
void
ide_rename_provider_rename_async (IdeRenameProvider   *self,
                                  IdeLocation         *location,
                                  const gchar         *new_name,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RENAME_PROVIDER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (new_name != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RENAME_PROVIDER_GET_IFACE (self)->rename_async (self, location, new_name, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_rename_provider_rename_finish:
 * @self: An #IdeRenameProvider
 * @result: a #GAsyncResult
 * @edits: (out) (transfer full) (element-type IdeTextEdit) (optional): A location
 *   for a #GPtrArray of #IdeTextEdit instances.
 * @error: a location for a #GError, or %NULL.
 *
 * Completes a request to ide_rename_provider_rename_async().
 *
 * You can use the resulting #GPtrArray of #IdeTextEdit instances to edit the
 * project to complete the symbol rename.
 *
 * Returns: %TRUE if successful and @edits is set. Otherwise %FALSE and @error
 *   is set.
 */
gboolean
ide_rename_provider_rename_finish (IdeRenameProvider  *self,
                                   GAsyncResult       *result,
                                   GPtrArray         **edits,
                                   GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RENAME_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  ret = IDE_RENAME_PROVIDER_GET_IFACE (self)->rename_finish (self, result, edits, error);

  IDE_RETURN (ret);
}

void
ide_rename_provider_load (IdeRenameProvider *self)
{
  g_return_if_fail (IDE_IS_RENAME_PROVIDER (self));

  if (IDE_RENAME_PROVIDER_GET_IFACE (self)->load)
    IDE_RENAME_PROVIDER_GET_IFACE (self)->load (self);
}

void
ide_rename_provider_unload (IdeRenameProvider *self)
{
  g_return_if_fail (IDE_IS_RENAME_PROVIDER (self));

  if (IDE_RENAME_PROVIDER_GET_IFACE (self)->unload)
    IDE_RENAME_PROVIDER_GET_IFACE (self)->unload (self);
}
