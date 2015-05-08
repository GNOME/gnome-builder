/* ide-thread-pool.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-thread-pool"

#include "egg-counter.h"

#include "ide-debug.h"
#include "ide-thread-pool.h"

#define COMPILER_MAX_THREADS 4

typedef struct
{
  GTask           *task;
  GTaskThreadFunc  func;
} WorkItem;

EGG_DEFINE_COUNTER (TotalTasks, "ThreadPool", "Total Tasks", "Total number of tasks processed.")
EGG_DEFINE_COUNTER (QueuedTasks, "ThreadPool", "Queued Tasks", "Current number of pending tasks.")

static GThreadPool *gThreadPools [IDE_THREAD_POOL_LAST];

static inline GThreadPool *
ide_thread_pool_get_pool (IdeThreadPoolKind kind)
{
  return gThreadPools [kind];
}

/**
 * ide_thread_pool_push_task:
 * @kind: The task kind.
 * @task: A #GTask to execute.
 * @func: (scope async): The thread worker to execute for @task.
 *
 * This pushes a task to be executed on a worker thread based on the task kind as denoted by
 * @kind. Some tasks will be placed on special work queues or throttled based on proirity.
 */
void
ide_thread_pool_push_task (IdeThreadPoolKind  kind,
                           GTask             *task,
                           GTaskThreadFunc    func)
{
  GThreadPool *pool;

  IDE_ENTRY;

  g_return_if_fail (kind >= 0);
  g_return_if_fail (kind < IDE_THREAD_POOL_LAST);
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (func != NULL);

  EGG_COUNTER_INC (TotalTasks);

  pool = ide_thread_pool_get_pool (kind);

  if (pool != NULL)
    {
      WorkItem *work_item;

      work_item = g_slice_new0 (WorkItem);
      work_item->task = g_object_ref (task);
      work_item->func = func;

      EGG_COUNTER_INC (QueuedTasks);

      g_thread_pool_push (pool, work_item, NULL);
    }
  else
    {
      g_task_run_in_thread (task, func);
    }

  IDE_EXIT;
}

static void
ide_thread_pool_worker (gpointer data,
                        gpointer user_data)
{
  WorkItem *work_item = data;
  gpointer source_object;
  gpointer task_data;
  GCancellable *cancellable;

  g_assert (work_item != NULL);

  EGG_COUNTER_DEC (QueuedTasks);

  source_object = g_task_get_source_object (work_item->task);
  task_data = g_task_get_task_data (work_item->task);
  cancellable = g_task_get_cancellable (work_item->task);

  work_item->func (work_item->task, source_object, task_data, cancellable);

  g_object_unref (work_item->task);
  g_slice_free (WorkItem, work_item);
}

void
_ide_thread_pool_init (void)
{
  /*
   * Create our thread pool exclusive to compiler tasks (such as those from Clang).
   * We don't want to consume threads fro other GTask's such as those regarding IO so we manage
   * these work items exclusively.
   */
  gThreadPools [IDE_THREAD_POOL_COMPILER] = g_thread_pool_new (ide_thread_pool_worker,
                                                               NULL,
                                                               COMPILER_MAX_THREADS,
                                                               TRUE,
                                                               NULL);
}
