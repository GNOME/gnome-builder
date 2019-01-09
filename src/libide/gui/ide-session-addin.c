/* ide-session-addin.c
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

#define G_LOG_DOMAIN "ide-session-addin"

#include "config.h"

#include "ide-session-addin.h"

G_DEFINE_INTERFACE (IdeSessionAddin, ide_session_addin, IDE_TYPE_OBJECT)

static void
ide_session_addin_real_save_async (IdeSessionAddin     *self,
                                   IdeWorkbench         *workbench,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_session_addin_real_save_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Save not supported");
}

static GVariant *
ide_session_addin_real_save_finish (IdeSessionAddin  *self,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_session_addin_real_restore_async (IdeSessionAddin     *self,
                                      IdeWorkbench         *workbench,
                                      GVariant            *state,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_session_addin_real_restore_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Restore not supported");
}

static gboolean
ide_session_addin_real_restore_finish (IdeSessionAddin  *self,
                                       GAsyncResult     *result,
                                       GError          **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_session_addin_default_init (IdeSessionAddinInterface *iface)
{
  iface->save_async = ide_session_addin_real_save_async;
  iface->save_finish = ide_session_addin_real_save_finish;
  iface->restore_async = ide_session_addin_real_restore_async;
  iface->restore_finish = ide_session_addin_real_restore_finish;
}

/**
 * ide_session_addin_save_async:
 * @self: a #IdeSessionAddin
 * @workbench: an #IdeWorkbench
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronous request to save state about the session.
 *
 * The resulting state will be provided when restoring the addin
 * at a future time.
 *
 * Since: 3.30
 */
void
ide_session_addin_save_async (IdeSessionAddin     *self,
                              IdeWorkbench        *workbench,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SESSION_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SESSION_ADDIN_GET_IFACE (self)->save_async (self, workbench, cancellable, callback, user_data);
}

/**
 * ide_session_addin_save_finish:
 * @self: a #IdeSessionAddin
 *
 * Completes an asynchronous request to save session state.
 *
 * The resulting #GVariant will be used to restore state at a future time.
 *
 * Returns: (transfer full) (nullable): a #GVariant or %NULL.
 *
 * Since: 3.30
 */
GVariant *
ide_session_addin_save_finish (IdeSessionAddin  *self,
                               GAsyncResult     *result,
                               GError          **error)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->save_finish (self, result, error);
}

/**
 * ide_session_addin_restore_async:
 * @self: a #IdeSessionAddin
 * @workbench: an #IdeWorkbench
 * @state: a #GVariant of previous state
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronous request to restore session state by the addin.
 *
 * Since: 3.30
 */
void
ide_session_addin_restore_async (IdeSessionAddin     *self,
                                 IdeWorkbench        *workbench,
                                 GVariant            *state,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SESSION_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SESSION_ADDIN_GET_IFACE (self)->restore_async (self, workbench, state, cancellable, callback, user_data);
}

gboolean
ide_session_addin_restore_finish (IdeSessionAddin  *self,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  g_return_val_if_fail (IDE_IS_SESSION_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_SESSION_ADDIN_GET_IFACE (self)->restore_finish (self, result, error);
}
