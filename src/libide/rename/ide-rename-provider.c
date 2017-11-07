/* ide-rename-provider.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-rename-provider.h"

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "rename/ide-rename-provider.h"

G_DEFINE_INTERFACE (IdeRenameProvider, ide_rename_provider, IDE_TYPE_OBJECT)

static void
ide_rename_provider_real_rename_async (IdeRenameProvider   *self,
                                       IdeSourceLocation   *location,
                                       const gchar         *new_name,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_RENAME_PROVIDER (self));
  g_assert (location != NULL);
  g_assert (new_name != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_rename_provider_real_rename_async);

  g_task_return_new_error (task,
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
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_rename_provider_default_init (IdeRenameProviderInterface *iface)
{
  iface->rename_async = ide_rename_provider_real_rename_async;
  iface->rename_finish = ide_rename_provider_real_rename_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("buffer",
                                                            "Buffer",
                                                            "Buffer",
                                                            IDE_TYPE_BUFFER,
                                                            (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));
}

/**
 * ide_rename_provider_rename_async:
 * @self: An #IdeRenameProvider
 * @location: An #IdeSourceLocation
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
                                  IdeSourceLocation   *location,
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
 * @edits: (out) (transfer container) (element-type Ide.ProjectEdit) (nullable): A location
 *   for a #GPtrArray of #IdeProjectEdit instances.
 * @error: a location for a #GError, or %NULL.
 *
 * Completes a request to ide_rename_provider_rename_async().
 *
 * You can use the resulting #GPtrArray of #IdeProjectEdit instances to edit the project
 * to complete the symbol rename.
 *
 * Returns: %TRUE if successful and @edits is set. Otherwise %FALSE and @error is set.
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
