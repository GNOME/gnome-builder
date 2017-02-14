/* ide-transfer.c
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

#define G_LOG_DOMAIN "ide-transfer"

#include "ide-transfer.h"

G_DEFINE_INTERFACE (IdeTransfer, ide_transfer, G_TYPE_OBJECT)

static void
ide_transfer_real_execute_async (IdeTransfer         *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_transfer_real_execute_finish (IdeTransfer   *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_TRANSFER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_transfer_default_init (IdeTransferInterface *iface)
{
  iface->execute_async = ide_transfer_real_execute_async;
  iface->execute_finish = ide_transfer_real_execute_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("title",
                                                            "Title",
                                                            "Title",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("icon-name",
                                                            "Icon Name",
                                                            "Icon Name",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_double ("progress",
                                                            "Progress",
                                                            "Progress",
                                                            0.0,
                                                            1.0,
                                                            0.0,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("status",
                                                            "Status",
                                                            "Status",
                                                            NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

void
ide_transfer_execute_async (IdeTransfer         *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_assert (IDE_IS_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TRANSFER_GET_IFACE (self)->execute_async (self, cancellable, callback, user_data);
}

gboolean
ide_transfer_execute_finish (IdeTransfer   *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_TRANSFER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_TRANSFER_GET_IFACE (self)->execute_finish (self, result, error);
}

gdouble
ide_transfer_get_progress (IdeTransfer *self)
{
  gdouble value = 0.0;

  g_return_val_if_fail (IDE_IS_TRANSFER (self), 0.0);

  if (ide_transfer_has_completed (self))
    return 1.0;

  g_object_get (self, "progress", &value, NULL);

  return value;
}

gboolean
ide_transfer_has_completed (IdeTransfer *self)
{
  g_return_val_if_fail (IDE_IS_TRANSFER (self), FALSE);

  if (IDE_TRANSFER_GET_IFACE (self)->has_completed)
    return IDE_TRANSFER_GET_IFACE (self)->has_completed (self);

  return !!g_object_get_data (G_OBJECT (self), "IDE_TRANSFER_COMPLETED");
}
