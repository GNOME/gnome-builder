/* ide-thread-pool.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-thread-pool"

#include "config.h"

#include <libide-core.h>

#include "ide-thread-pool.h"
#include "ide-thread-private.h"

typedef struct
{
  int type;
  int priority;
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

struct _IdeThreadPool
{
  GThreadPool       *pool;
  IdeThreadPoolKind  kind;
  guint              max_threads;
  guint              worker_max_threads;
  gboolean           exclusive;
};

static IdeThreadPool thread_pools[] = {
  { NULL, IDE_THREAD_POOL_DEFAULT, 10, 1, FALSE },
  { NULL, IDE_THREAD_POOL_COMPILER, 8, 8, FALSE },
  { NULL, IDE_THREAD_POOL_INDEXER,  1, 1, FALSE },
  { NULL, IDE_THREAD_POOL_IO,       8, 1, FALSE },
  { NULL, IDE_THREAD_POOL_LAST,     0, 0, FALSE }
};

enum {
  TYPE_TASK,
  TYPE_FUNC,
};

static inline GThreadPool *
ide_thread_pool_get_pool (IdeThreadPoolKind kind)
{
  /* Fallback to allow using without IdeApplication */
  if G_UNLIKELY (thread_pools [kind].pool == NULL)
    _ide_thread_pool_init (TRUE);

  return thread_pools [kind].pool;
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

  pool = ide_thread_pool_get_pool (kind);

  if (pool != NULL)
    {
      WorkItem *work_item;

      work_item = g_slice_new0 (WorkItem);
      work_item->type = TYPE_TASK;
      work_item->priority = g_task_get_priority (task);
      work_item->task.task = g_object_ref (task);
      work_item->task.func = func;

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
  ide_thread_pool_push_with_priority (kind, G_PRIORITY_DEFAULT, func, func_data);
}

/**
 * ide_thread_pool_push_with_priority:
 * @kind: the threadpool kind to use.
 * @priority: the priority for func
 * @func: (scope async) (closure func_data): A function to call in the worker thread.
 * @func_data: user data for @func.
 *
 * Runs the callback on the thread pool thread.
 */
void
ide_thread_pool_push_with_priority (IdeThreadPoolKind kind,
                                    gint              priority,
                                    IdeThreadFunc     func,
                                    gpointer          func_data)
{
  GThreadPool *pool;

  IDE_ENTRY;

  g_return_if_fail (kind >= 0);
  g_return_if_fail (kind < IDE_THREAD_POOL_LAST);
  g_return_if_fail (func != NULL);

  pool = ide_thread_pool_get_pool (kind);

  if (pool != NULL)
    {
      WorkItem *work_item;

      work_item = g_slice_new0 (WorkItem);
      work_item->type = TYPE_FUNC;
      work_item->priority = priority;
      work_item->func.callback = func;
      work_item->func.data = func_data;

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

static gint
thread_pool_sort_func (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
  const WorkItem *a_item = a;
  const WorkItem *b_item = b;

  return a_item->priority - b_item->priority;
}

void
_ide_thread_pool_init (gboolean is_worker)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      for (IdeThreadPoolKind kind = IDE_THREAD_POOL_DEFAULT;
           kind < IDE_THREAD_POOL_LAST;
           kind++)
        {
          IdeThreadPool *p = &thread_pools[kind];
          g_autoptr(GError) error = NULL;

          p->pool = g_thread_pool_new (ide_thread_pool_worker,
                                       NULL,
                                       is_worker ? p->worker_max_threads : p->max_threads,
                                       p->exclusive,
                                       &error);
          g_thread_pool_set_sort_function (p->pool, thread_pool_sort_func, NULL);

          if (error != NULL)
            g_error ("Failed to initialize thread pool %u: %s",
                     p->kind, error->message);
        }

      g_once_init_leave (&initialized, TRUE);
    }
}
