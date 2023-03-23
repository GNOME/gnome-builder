/* gbp-code-index-executor.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-code-index-executor"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-threading.h>
#include <libpeas.h>

#include "gbp-code-index-builder.h"
#include "gbp-code-index-executor.h"

struct _GbpCodeIndexExecutor
{
  IdeObject         parent_instance;
  GbpCodeIndexPlan *plan;
};

typedef struct
{
  GbpCodeIndexPlan *plan;
  IdeNotification  *notif;
  GFile            *cachedir;
  GFile            *workdir;
  GPtrArray        *builders;
  guint             pos;
  guint64           num_ops;
  guint64           num_completed;
} Execute;

G_DEFINE_FINAL_TYPE (GbpCodeIndexExecutor, gbp_code_index_executor, IDE_TYPE_OBJECT)

static void
execute_free (Execute *exec)
{
  g_clear_object (&exec->plan);
  g_clear_object (&exec->notif);
  g_clear_object (&exec->cachedir);
  g_clear_object (&exec->workdir);
  g_clear_pointer (&exec->builders, g_ptr_array_unref);
  g_slice_free (Execute, exec);
}

GbpCodeIndexExecutor *
gbp_code_index_executor_new (GbpCodeIndexPlan *plan)
{
  GbpCodeIndexExecutor *self;

  g_return_val_if_fail (GBP_IS_CODE_INDEX_PLAN (plan), NULL);

  self = g_object_new (GBP_TYPE_CODE_INDEX_EXECUTOR, NULL);
  self->plan = g_object_ref (plan);

  return g_steal_pointer (&self);
}

static void
gbp_code_index_executor_finalize (GObject *object)
{
  GbpCodeIndexExecutor *self = (GbpCodeIndexExecutor *)object;

  g_clear_object (&self->plan);

  G_OBJECT_CLASS (gbp_code_index_executor_parent_class)->finalize (object);
}

static void
gbp_code_index_executor_class_init (GbpCodeIndexExecutorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_code_index_executor_finalize;
}

static void
gbp_code_index_executor_init (GbpCodeIndexExecutor *self)
{
}

static gboolean
execute_count_ops_cb (GFile              *directory,
                      GPtrArray          *plan_items,
                      GbpCodeIndexReason  reason,
                      gpointer            user_data)
{
  guint64 *count = user_data;
  (*count)++;
  return FALSE;
}

static guint64
execute_count_ops (GbpCodeIndexPlan *plan)
{
  guint64 count = 0;
  gbp_code_index_plan_foreach (plan, execute_count_ops_cb, &count);
  return count;
}

static gboolean
gbp_code_index_executor_collect_cb (GFile              *directory,
                                    GPtrArray          *plan_items,
                                    GbpCodeIndexReason  reason,
                                    gpointer            user_data)
{
  g_autoptr(GbpCodeIndexBuilder) builder = NULL;
  g_autoptr(GFile) index_dir = NULL;
  g_autofree gchar *relative = NULL;
  IdeTask *task = user_data;
  GbpCodeIndexExecutor *self;
  Execute *state;

  g_assert (G_IS_FILE (directory));
  g_assert (plan_items != NULL);
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  relative = g_file_get_relative_path (state->workdir, directory);

  if (relative == NULL)
    index_dir = g_object_ref (state->cachedir);
  else
    index_dir = g_file_get_child (state->cachedir, relative);

  if (reason == GBP_CODE_INDEX_REASON_REMOVE_INDEX)
    {
      g_autoptr(GFile) names = g_file_get_child (index_dir, "SymbolNames");
      g_autoptr(GFile) keys = g_file_get_child (index_dir, "SymbolKeys");

      g_file_delete_async (names, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
      g_file_delete_async (keys, G_PRIORITY_DEFAULT, NULL, NULL, NULL);

      state->num_completed++;

      ide_notification_set_progress (state->notif,
                                     (gdouble)state->num_completed / (gdouble)state->num_ops);

      return FALSE;
    }

  builder = gbp_code_index_builder_new (directory, index_dir);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (builder));

  for (guint i = 0; i < plan_items->len; i++)
    gbp_code_index_builder_add_item (builder, g_ptr_array_index (plan_items, i));

  g_ptr_array_add (state->builders, g_steal_pointer (&builder));

  return FALSE;
}

static void
gbp_code_index_executor_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GbpCodeIndexBuilder *builder = (GbpCodeIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Execute *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (builder));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  gbp_code_index_builder_run_finish (builder, result, &error);

  state = ide_task_get_task_data (task);

  state->pos++;
  state->num_completed++;

  ide_notification_set_progress (state->notif,
                                 (gdouble)state->num_completed / (gdouble)state->num_ops);

  if (state->pos >= state->builders->len)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  gbp_code_index_builder_run_async (g_ptr_array_index (state->builders, state->pos),
                                    ide_task_get_cancellable (task),
                                    gbp_code_index_executor_run_cb,
                                    g_object_ref (task));
}

void
gbp_code_index_executor_execute_async (GbpCodeIndexExecutor *self,
                                       IdeNotification      *notif,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeContext) context = NULL;
  Execute *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_EXECUTOR (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_executor_execute_async);

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Not connected to object tree");
      IDE_EXIT;
    }

  state = g_slice_new0 (Execute);
  state->plan = g_object_ref (self->plan);
  state->notif = notif ? g_object_ref (notif) : ide_notification_new ();
  state->num_ops = execute_count_ops (self->plan);
  state->builders = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);
  state->cachedir = ide_context_cache_file (context, "code-index", NULL);
  state->workdir = ide_context_ref_workdir (context);
  state->pos = 0;
  ide_task_set_task_data (task, state, execute_free);

  ide_notification_set_has_progress (state->notif, TRUE);
  ide_notification_set_progress (state->notif, 0.0);
  ide_notification_set_progress_is_imprecise (state->notif, FALSE);

  gbp_code_index_plan_foreach (self->plan,
                               gbp_code_index_executor_collect_cb,
                               task);

  if (state->builders->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  gbp_code_index_builder_run_async (g_ptr_array_index (state->builders, 0),
                                    cancellable,
                                    gbp_code_index_executor_run_cb,
                                    g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_code_index_executor_execute_finish (GbpCodeIndexExecutor  *self,
                                        GAsyncResult          *result,
                                        GError               **error)
{
  Execute *state;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_EXECUTOR (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  state = ide_task_get_task_data (IDE_TASK (result));

  if ((ret = ide_task_propagate_boolean (IDE_TASK (result), error)))
    ide_notification_set_progress (state->notif, 1.0);

  IDE_RETURN (ret);
}
