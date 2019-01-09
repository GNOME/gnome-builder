/* ide-dependency-updater.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-dependency-updater"

#include "config.h"

#include "ide-dependency-updater.h"

G_DEFINE_INTERFACE (IdeDependencyUpdater, ide_dependency_updater, IDE_TYPE_OBJECT)

static void
ide_dependency_updater_real_update_async (IdeDependencyUpdater *self,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_dependency_updater_real_update_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "update_async is not supported");
}

static gboolean
ide_dependency_updater_real_update_finish (IdeDependencyUpdater  *self,
                                           GAsyncResult          *result,
                                           GError               **error)
{
  g_assert (IDE_IS_DEPENDENCY_UPDATER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_dependency_updater_default_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = ide_dependency_updater_real_update_async;
  iface->update_finish = ide_dependency_updater_real_update_finish;
}

void
ide_dependency_updater_update_async (IdeDependencyUpdater *self,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
  g_return_if_fail (IDE_IS_DEPENDENCY_UPDATER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEPENDENCY_UPDATER_GET_IFACE (self)->update_async (self, cancellable, callback, user_data);
}

gboolean
ide_dependency_updater_update_finish (IdeDependencyUpdater  *self,
                                      GAsyncResult          *result,
                                      GError               **error)
{
  g_return_val_if_fail (IDE_IS_DEPENDENCY_UPDATER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEPENDENCY_UPDATER_GET_IFACE (self)->update_finish (self, result, error);
}
