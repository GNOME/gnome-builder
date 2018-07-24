/* ide-async-helper.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "util/ide-async-helper.h"
#include "threading/ide-task.h"

static void
ide_async_helper_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GPtrArray *funcs;

  g_return_if_fail (IDE_IS_TASK (task));
  g_return_if_fail (IDE_IS_TASK (result) || G_IS_TASK (result));

  if (IDE_IS_TASK (result))
    {
      if (!ide_task_propagate_boolean (IDE_TASK (result), &error))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }
  else
    {
      g_assert (G_IS_TASK (result));

      if (!g_task_propagate_boolean (G_TASK (result), &error))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  funcs = ide_task_get_task_data (task);

  g_ptr_array_remove_index (funcs, 0);

  if (funcs->len)
    {
      IdeAsyncStep step;

      step = g_ptr_array_index (funcs, 0);
      step (ide_task_get_source_object (task),
            ide_task_get_cancellable (task),
            ide_async_helper_cb,
            g_object_ref (task));
    }
  else
    ide_task_return_boolean (task, TRUE);
}

void
ide_async_helper_run (gpointer             source_object,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data,
                      IdeAsyncStep         step1,
                      ...)
{
  g_autoptr(IdeTask) task = NULL;
  IdeAsyncStep step;
  GPtrArray *funcs;
  va_list args;

  g_return_if_fail (step1);

  funcs = g_ptr_array_new ();
  va_start (args, step1);
  for (step = step1; step; step = va_arg (args, IdeAsyncStep))
    g_ptr_array_add (funcs, step);
  va_end (args);

  task = ide_task_new (source_object, cancellable, callback, user_data);
  ide_task_set_task_data (task, funcs, g_ptr_array_unref);

  step1 (source_object,
         cancellable,
         ide_async_helper_cb,
         g_object_ref (task));
}
