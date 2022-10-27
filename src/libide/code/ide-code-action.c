/* ide-code-action.c
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

#define G_LOG_DOMAIN "ide-code-action"

#include "config.h"

#include "ide-buffer.h"
#include "ide-code-action.h"

G_DEFINE_INTERFACE (IdeCodeAction, ide_code_action, G_TYPE_OBJECT)

static char *
ide_code_action_real_get_title (IdeCodeAction *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_ACTION (self));

  return NULL;
}

static void
ide_code_action_real_execute_async (IdeCodeAction       *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_assert (IDE_IS_CODE_ACTION (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_code_action_real_execute_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "The operation is not supported");
}

static gboolean
ide_code_action_real_execute_finish (IdeCodeAction  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (IDE_IS_CODE_ACTION (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean(G_TASK (result), error);
}

static void
ide_code_action_default_init (IdeCodeActionInterface *iface)
{
  iface->get_title = ide_code_action_real_get_title;
  iface->execute_async = ide_code_action_real_execute_async;
  iface->execute_finish = ide_code_action_real_execute_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_string ("title", NULL, NULL, NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

char *
ide_code_action_get_title (IdeCodeAction *self)
{
  g_return_val_if_fail (IDE_IS_CODE_ACTION (self), NULL);

  return IDE_CODE_ACTION_GET_IFACE (self)->get_title (self);
}

void
ide_code_action_execute_async (IdeCodeAction       *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (IDE_IS_CODE_ACTION (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CODE_ACTION_GET_IFACE (self)->execute_async (self, cancellable, callback, user_data);
}

gboolean
ide_code_action_execute_finish (IdeCodeAction  *self,
                                GAsyncResult   *result,
                                GError        **error)
{
  g_return_val_if_fail (IDE_IS_CODE_ACTION (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_CODE_ACTION_GET_IFACE (self)->execute_finish (self, result, error);
}
