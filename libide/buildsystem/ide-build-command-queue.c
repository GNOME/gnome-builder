/* ide-build-command-queue.c
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

#define G_LOG_DOMAIN "ide-build-command-queue"

#include "ide-debug.h"

#include "buildsystem/ide-build-command.h"
#include "buildsystem/ide-build-command-queue.h"
#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-environment.h"
#include "runtimes/ide-runtime.h"

typedef struct
{
  const GList    *iter;
  IdeRuntime     *runtime;
  IdeEnvironment *environment;
  IdeBuildResult *build_result;
} ExecuteState;

struct _IdeBuildCommandQueue
{
  GObject parent_instance;
  GQueue  queue;
};

static void list_model_iface_init                (GListModelInterface *iface);
static void ide_build_command_queue_execute_pump (GTask               *task);

G_DEFINE_TYPE_EXTENDED (IdeBuildCommandQueue, ide_build_command_queue, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
execute_state_free (gpointer data)
{
  ExecuteState *state = data;

  if (state != NULL)
    {
      g_clear_object (&state->runtime);
      g_clear_object (&state->environment);
      g_clear_object (&state->build_result);
      g_slice_free (ExecuteState, state);
    }
}

static void
ide_build_command_queue_finalize (GObject *object)
{
  IdeBuildCommandQueue *self = (IdeBuildCommandQueue *)object;

  g_queue_foreach (&self->queue, (GFunc)g_object_unref, NULL);
  g_queue_clear (&self->queue);

  G_OBJECT_CLASS (ide_build_command_queue_parent_class)->finalize (object);
}

static void
ide_build_command_queue_class_init (IdeBuildCommandQueueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_command_queue_finalize;
}

static void
ide_build_command_queue_init (IdeBuildCommandQueue *self)
{
  g_queue_init (&self->queue);
}

void
ide_build_command_queue_append (IdeBuildCommandQueue *self,
                                IdeBuildCommand      *command)
{
  g_return_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self));
  g_return_if_fail (IDE_IS_BUILD_COMMAND (command));

  g_queue_push_tail (&self->queue, g_object_ref (command));
}

static GType
ide_build_command_queue_get_item_type (GListModel *model)
{
  return IDE_TYPE_BUILD_COMMAND;
}

static guint
ide_build_command_queue_get_n_items (GListModel *model)
{
  IdeBuildCommandQueue *self = (IdeBuildCommandQueue *)model;

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self), 0);

  return self->queue.length;
}

static gpointer
ide_build_command_queue_get_item (GListModel *model,
                                  guint       position)
{
  IdeBuildCommandQueue *self = (IdeBuildCommandQueue *)model;

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self), NULL);
  g_return_val_if_fail (position < self->queue.length, NULL);

  return g_object_ref (g_queue_peek_nth (&self->queue, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_build_command_queue_get_item_type;
  iface->get_n_items = ide_build_command_queue_get_n_items;
  iface->get_item = ide_build_command_queue_get_item;
}

IdeBuildCommandQueue *
ide_build_command_queue_new (void)
{
  return g_object_new (IDE_TYPE_BUILD_COMMAND_QUEUE, NULL);
}

static void
ide_build_command_queue_execute_run_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBuildCommand *build_command = (IdeBuildCommand *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_BUILD_COMMAND (build_command));
  g_assert (G_IS_TASK (task));

  if (!ide_build_command_run_finish (build_command, result, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  ide_build_command_queue_execute_pump (task);
}

static void
ide_build_command_queue_execute_pump (GTask *task)
{
  ExecuteState *state;
  IdeBuildCommand *command;

  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (IDE_IS_RUNTIME (state->runtime));
  g_assert (IDE_IS_ENVIRONMENT (state->environment));
  g_assert (IDE_IS_BUILD_RESULT (state->build_result));

  if (state->iter == NULL)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  command = state->iter->data;
  state->iter = state->iter->next;

  ide_build_command_run_async (command,
                               state->runtime,
                               state->environment,
                               state->build_result,
                               g_task_get_cancellable (task),
                               ide_build_command_queue_execute_run_cb,
                               g_object_ref (task));
}

gboolean
ide_build_command_queue_execute (IdeBuildCommandQueue  *self,
                                 IdeRuntime            *runtime,
                                 IdeEnvironment        *environment,
                                 IdeBuildResult        *build_result,
                                 GCancellable          *cancellable,
                                 GError               **error)
{
  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), FALSE);
  g_return_val_if_fail (IDE_IS_ENVIRONMENT (environment), FALSE);
  g_return_val_if_fail (IDE_IS_BUILD_RESULT (build_result), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  for (const GList *iter = self->queue.head; iter != NULL; iter = iter->next)
    {
      IdeBuildCommand *build_command = iter->data;

      g_assert (IDE_IS_BUILD_COMMAND (build_command));

      if (!ide_build_command_run (build_command, runtime, environment, build_result, cancellable, error))
        IDE_RETURN (FALSE);
    }

  IDE_RETURN (TRUE);
}

void
ide_build_command_queue_execute_async (IdeBuildCommandQueue *self,
                                       IdeRuntime           *runtime,
                                       IdeEnvironment       *environment,
                                       IdeBuildResult       *build_result,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  ExecuteState *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));
  g_return_if_fail (IDE_IS_ENVIRONMENT (environment));
  g_return_if_fail (IDE_IS_BUILD_RESULT (build_result));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_command_queue_execute_async);

  state = g_slice_new (ExecuteState);
  state->runtime = g_object_ref (runtime);
  state->environment = g_object_ref (environment);
  state->build_result = g_object_ref (build_result);
  state->iter = self->queue.head;

  g_task_set_task_data (task, state, execute_state_free);

  ide_build_command_queue_execute_pump (task);

  IDE_EXIT;
}

gboolean
ide_build_command_queue_execute_finish (IdeBuildCommandQueue  *self,
                                        GAsyncResult          *result,
                                        GError               **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_build_command_queue_copy:
 *
 * Returns: (transfer full): An #IdeBuildCommandQueue
 */
IdeBuildCommandQueue *
ide_build_command_queue_copy (IdeBuildCommandQueue *self)
{
  IdeBuildCommandQueue *ret;

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND_QUEUE (self), NULL);

  ret = g_object_new (IDE_TYPE_BUILD_COMMAND_QUEUE, NULL);

  for (const GList *iter = self->queue.head; iter; iter = iter->next)
    {
      IdeBuildCommand *command = iter->data;

      g_queue_push_tail (&ret->queue, ide_build_command_copy (command));
    }

  return ret;
}
