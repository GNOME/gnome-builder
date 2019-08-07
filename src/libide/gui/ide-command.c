/* ide-command.c
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

#define G_LOG_DOMAIN "ide-command"

#include "config.h"

#include "ide-command.h"

G_DEFINE_INTERFACE (IdeCommand, ide_command, IDE_TYPE_OBJECT)

static void
ide_command_real_run_async (IdeCommand          *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_COMMAND (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_command_real_run_async);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "The operation is not supported");
}

static gboolean
ide_command_real_run_finish (IdeCommand    *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_command_default_init (IdeCommandInterface *iface)
{
  iface->run_async = ide_command_real_run_async;
  iface->run_finish = ide_command_real_run_finish;
}

/**
 * ide_command_run_async:
 * @self: an #IdeCommand
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Runs the command, asynchronously.
 *
 * Use ide_command_run_finish() to get the result of the operation.
 *
 * Since: 3.32
 */
void
ide_command_run_async (IdeCommand          *self,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  g_return_if_fail (IDE_IS_COMMAND (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_COMMAND_GET_IFACE (self)->run_async (self, cancellable, callback, user_data);
}

/**
 * ide_command_run_finish:
 * @self: an #IdeCommand
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: %TRUE if the command was successful; otherwise %FALSE
 *   and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_command_run_finish (IdeCommand    *self,
                        GAsyncResult  *result,
                        GError       **error)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_COMMAND_GET_IFACE (self)->run_finish (self, result, error);
}

/**
 * ide_command_get_title:
 * @self: a #IdeCommand
 *
 * Gets the title for the command.
 *
 * Returns: a string containing the title
 *
 * Since: 3.32
 */
gchar *
ide_command_get_title (IdeCommand *self)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), NULL);

  if (IDE_COMMAND_GET_IFACE (self)->get_title)
    return IDE_COMMAND_GET_IFACE (self)->get_title (self);

  return NULL;
}

/**
 * ide_command_get_subtitle:
 * @self: a #IdeCommand
 *
 * Gets the subtitle for the command.
 *
 * Returns: a string containing the subtitle
 *
 * Since: 3.32
 */
gchar *
ide_command_get_subtitle (IdeCommand *self)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), NULL);

  if (IDE_COMMAND_GET_IFACE (self)->get_subtitle)
    return IDE_COMMAND_GET_IFACE (self)->get_subtitle (self);

  return NULL;
}

/**
 * ide_command_get_priority:
 * @self: a #IdeCommand
 *
 * Gets the priority for the command.
 *
 * This is generally just useful when using the command bar so that the items
 * may be sorted in a useful manner.
 *
 * Command providers may want to use the typed_text for the query operation
 * to calculate a score with fuzzy matching.
 *
 * The lower the value, the higher priority.
 *
 * Returns: an integer with the sort priority
 *
 * Since: 3.34
 */
gint
ide_command_get_priority (IdeCommand *self)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), G_MAXINT);

  if (IDE_COMMAND_GET_IFACE (self)->get_priority)
    return IDE_COMMAND_GET_IFACE (self)->get_priority (self);

  return G_MAXINT;
}

/**
 * ide_command_get_icon:
 * @self: a #IdeCommand
 *
 * Gets the icon for the command to be displayed in UI if necessary.
 *
 * Returns: (transfer full) (nullable): a #GIcon or %NULL
 *
 * Since: 3.34
 */
GIcon *
ide_command_get_icon (IdeCommand *self)
{
  g_return_val_if_fail (IDE_IS_COMMAND (self), NULL);

  if (IDE_COMMAND_GET_IFACE (self)->get_icon)
    return IDE_COMMAND_GET_IFACE (self)->get_icon (self);

  return NULL;
}
