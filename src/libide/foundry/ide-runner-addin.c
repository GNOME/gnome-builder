/* ide-runner-addin.c
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

#define G_LOG_DOMAIN "ide-runner-addin"

#include "config.h"

#include <libide-threading.h>

#include "ide-runner.h"
#include "ide-runner-addin.h"

G_DEFINE_INTERFACE (IdeRunnerAddin, ide_runner_addin, G_TYPE_OBJECT)

static void
ide_runner_addin_real_load (IdeRunnerAddin *self,
                            IdeRunner      *runner)
{
}

static void
ide_runner_addin_real_unload (IdeRunnerAddin *self,
                              IdeRunner      *runner)
{
}

static void
dummy_async (IdeRunnerAddin      *self,
             GCancellable        *cancellable,
             GAsyncReadyCallback  callback,
             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_RUNNER_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (callback == NULL)
    return;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
dummy_finish (IdeRunnerAddin  *self,
              GAsyncResult    *result,
              GError         **error)
{
  g_assert (IDE_IS_RUNNER_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_runner_addin_default_init (IdeRunnerAddinInterface *iface)
{
  iface->load = ide_runner_addin_real_load;
  iface->unload = ide_runner_addin_real_unload;
  iface->prehook_async = dummy_async;
  iface->prehook_finish = dummy_finish;
  iface->posthook_async = dummy_async;
  iface->posthook_finish = dummy_finish;
}

void
ide_runner_addin_load (IdeRunnerAddin *self,
                       IdeRunner      *runner)
{
  g_assert (IDE_IS_RUNNER_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));

  IDE_RUNNER_ADDIN_GET_IFACE (self)->load (self, runner);
}

void
ide_runner_addin_unload (IdeRunnerAddin *self,
                         IdeRunner      *runner)
{
  g_assert (IDE_IS_RUNNER_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));

  IDE_RUNNER_ADDIN_GET_IFACE (self)->unload (self, runner);
}

void
ide_runner_addin_prehook_async (IdeRunnerAddin      *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNNER_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNNER_ADDIN_GET_IFACE (self)->prehook_async (self, cancellable, callback, user_data);
}

gboolean
ide_runner_addin_prehook_finish (IdeRunnerAddin  *self,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  g_return_val_if_fail (IDE_IS_RUNNER_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_RUNNER_ADDIN_GET_IFACE (self)->prehook_finish (self, result, error);
}

void
ide_runner_addin_posthook_async (IdeRunnerAddin      *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNNER_ADDIN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNNER_ADDIN_GET_IFACE (self)->posthook_async (self, cancellable, callback, user_data);
}

gboolean
ide_runner_addin_posthook_finish (IdeRunnerAddin  *self,
                                  GAsyncResult    *result,
                                  GError         **error)
{
  g_return_val_if_fail (IDE_IS_RUNNER_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_RUNNER_ADDIN_GET_IFACE (self)->posthook_finish (self, result, error);
}
