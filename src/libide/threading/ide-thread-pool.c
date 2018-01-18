/* ide-thread-pool.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <dazzle.h>

#include "ide-debug.h"

#include "threading/ide-thread-pool.h"

#define COMPILER_MAX_THREADS (g_get_num_processors())
#define INDEXER_MAX_THREADS  (MAX (1, g_get_num_processors() / 2))

typedef struct
{
  int type;
  union {
    struct {
      GTask           *task;
      GTaskThreadFunc  func;
    } task;
    struct {
      IdeThreadFunc callback;
      gpointer      data;
    } func;
  };
} WorkItem;

DZL_DEFINE_COUNTER (TotalTasks, "ThreadPool", "Total Tasks", "Total number of tasks processed.")
DZL_DEFINE_COUNTER (QueuedTasks, "ThreadPool", "Queued Tasks", "Current number of pending tasks.")

static GThreadPool *thread_pools [IDE_THREAD_POOL_LAST];

enum {
  TYPE_TASK,
  TYPE_FUNC,
};

static inline GThreadPool *
ide_thread_pool_get_pool (IdeThreadPoolKind kind)
{
  return thread_pools [kind];
}

/**
 * ide_thread_pool_push_task:
 * @kind: The task kind.
 * @task: a #GTask to execute.
 * @func: (scope async): The thread worker to execute for @task.
 *
 * This pushes a task to be executed on a worker thread based on the task kind as denoted by
 * @kind. Some tasks will be placed on special work queues or throttled based on priority.
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

  DZL_COUNTER_INC (TotalTasks);

  pool = ide_thread_pool_get_pool (kind);

  if (pool != NULL)
    {
      WorkItem *work_item;

      work_item = g_slice_new0 (WorkItem);
      work_item->type = TYPE_TASK;
      work_item->task.task = g_object_ref (task);
      work_item->task.func = func;

      DZL_COUNTER_INC (QueuedTasks);

      g_thread_pool_push (pool, work_item, NULL);
    }
  else
    {
      g_task_run_in_thread (task, func);
    }

  IDE_EXIT;
}

/**
 * ide_thread_pool_push:
 * @kind: the threadpool kind to use.
 * @func: (scope async) (closure func_data): A function to call in the worker thread.
 * @func_data: user data for @func.
 *
 * Runs the callback on the thread pool thread.
 */
void
ide_thread_pool_push (IdeThreadPoolKind kind,
                      IdeThreadFunc     func,
                      gpointer          func_data)
{
  GThreadPool *pool;

  IDE_ENTRY;

  g_return_if_fail (kind >= 0);
  g_return_if_fail (kind < IDE_THREAD_POOL_LAST);
  g_return_if_fail (func != NULL);

  DZL_COUNTER_INC (TotalTasks);

  pool = ide_thread_pool_get_pool (kind);

  if (pool != NULL)
    {
      WorkItem *work_item;

      work_item = g_slice_new0 (WorkItem);
      work_item->type = TYPE_FUNC;
      work_item->func.callback = func;
      work_item->func.data = func_data;

      DZL_COUNTER_INC (QueuedTasks);

      g_thread_pool_push (pool, work_item, NULL);
    }
  else
    {
      g_critical ("No such thread pool %02x", kind);
    }

  IDE_EXIT;
}

static void
ide_thread_pool_worker (gpointer data,
                        gpointer user_data)
{
  WorkItem *work_item = data;

  g_assert (work_item != NULL);

  DZL_COUNTER_DEC (QueuedTasks);

  if (work_item->type == TYPE_TASK)
    {
      gpointer source_object = g_task_get_source_object (work_item->task.task);
      gpointer task_data = g_task_get_task_data (work_item->task.task);
      GCancellable *cancellable = g_task_get_cancellable (work_item->task.task);

      work_item->task.func (work_item->task.task, source_object, task_data, cancellable);
      g_clear_object (&work_item->task.task);
      work_item->task.func = NULL;
    }
  else if (work_item->type == TYPE_FUNC)
    {
      work_item->func.callback (work_item->func.data);
      work_item->func.data = NULL;
    }

  g_slice_free (WorkItem, work_item);
}

void
_ide_thread_pool_init (gboolean is_worker)
{
  gint compiler = COMPILER_MAX_THREADS;
  gint indexer = INDEXER_MAX_THREADS;
  gboolean exclusive = FALSE;

  if (is_worker)
    {
      compiler = 1;
      indexer = 1;
      exclusive = TRUE;
    }

  /*
   * Create our thread pool exclusive to compiler tasks (such as those from Clang).
   * We don't want to consume threads from other GTask's such as those regarding IO so we manage
   * these work items exclusively.
   */
  thread_pools [IDE_THREAD_POOL_COMPILER] = g_thread_pool_new (ide_thread_pool_worker,
                                                               NULL,
                                                               compiler,
                                                               exclusive,
                                                               NULL);

  /*
   * Create our pool exclusive to things like indexing. Such examples including building of
   * ctags indexes or highlight indexes.
   */
  thread_pools [IDE_THREAD_POOL_INDEXER] = g_thread_pool_new (ide_thread_pool_worker,
                                                              NULL,
                                                              indexer,
                                                              exclusive,
                                                              NULL);
}
