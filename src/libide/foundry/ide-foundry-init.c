/* ide-foundry-init.c
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

#define G_LOG_DOMAIN "ide-foundry-init"

#include "config.h"

#include <libide-threading.h>

#include "ide-build-manager.h"
#include "ide-device-manager.h"
#include "ide-configuration-manager.h"
#include "ide-foundry-init.h"
#include "ide-run-manager.h"
#include "ide-runtime-manager.h"
#include "ide-test-manager.h"
#include "ide-toolchain-manager.h"

typedef struct
{
  GQueue to_init;
} FoundryInit;

static void
foundry_init_free (FoundryInit *state)
{
  g_queue_foreach (&state->to_init, (GFunc)g_object_unref, NULL);
  g_queue_clear (&state->to_init);
  g_slice_free (FoundryInit, state);
}

static void
ide_foundry_init_async_cb (GObject      *init_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)init_object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  FoundryInit *state;
  GCancellable *cancellable;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_async_initable_init_finish (initable, result, &error))
    g_warning ("Failed to init %s: %s",
               G_OBJECT_TYPE_NAME (initable), error->message);

  state = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  while (state->to_init.head)
    {
      g_autoptr(IdeObject) object = g_queue_pop_head (&state->to_init);

      if (G_IS_ASYNC_INITABLE (object))
        {
          g_async_initable_init_async (G_ASYNC_INITABLE (object),
                                       G_PRIORITY_DEFAULT,
                                       cancellable,
                                       ide_foundry_init_async_cb,
                                       g_steal_pointer (&task));
          return;
        }

      if (G_IS_INITABLE (object))
        g_initable_init (G_INITABLE (object), NULL, NULL);
    }

  ide_task_return_boolean (task, TRUE);
}

void
_ide_foundry_init_async (IdeContext          *context,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  FoundryInit *state;
  GType foundry_types[] = {
    IDE_TYPE_DEVICE_MANAGER,
    IDE_TYPE_RUNTIME_MANAGER,
    IDE_TYPE_TOOLCHAIN_MANAGER,
    IDE_TYPE_CONFIGURATION_MANAGER,
    IDE_TYPE_BUILD_MANAGER,
    IDE_TYPE_RUN_MANAGER,
    IDE_TYPE_TEST_MANAGER,
  };

  g_return_if_fail (IDE_IS_CONTEXT (context));

  state = g_slice_new0 (FoundryInit);
  g_queue_init (&state->to_init);

  task = ide_task_new (context, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_foundry_init_async);
  ide_task_set_task_data (task, state, foundry_init_free);

  for (guint i = 0; i < G_N_ELEMENTS (foundry_types); i++)
    {
      g_autoptr(IdeObject) object = NULL;

      /* Skip if plugins already forced this subsystem to load */
      if ((object = ide_object_get_child_typed (IDE_OBJECT (context), foundry_types[i])))
        continue;

      object = g_object_new (foundry_types[i], NULL);
      ide_object_append (IDE_OBJECT (context), object);
      g_queue_push_tail (&state->to_init, g_steal_pointer (&object));
    }

  while (state->to_init.head)
    {
      g_autoptr(IdeObject) object = g_queue_pop_head (&state->to_init);

      if (G_IS_ASYNC_INITABLE (object))
        {
          g_async_initable_init_async (G_ASYNC_INITABLE (object),
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_foundry_init_async_cb,
                                       g_steal_pointer (&task));
          return;
        }

      if (G_IS_INITABLE (object))
        g_initable_init (G_INITABLE (object), NULL, NULL);
    }

  ide_task_return_boolean (task, TRUE);
}

gboolean
_ide_foundry_init_finish (GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
