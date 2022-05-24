/* ide-task.c
 *
 * Copyright 2018 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define G_LOG_DOMAIN "ide-task"

#include "config.h"

#include <libide-core.h>

#include "ide-task.h"
#include "ide-thread-pool.h"
#include "ide-thread-private.h"

/* From GDK_PRIORITY_REDRAW */
#define PRIORITY_REDRAW (G_PRIORITY_HIGH_IDLE + 20)

#if 0
# define ENABLE_TIME_CHART
#endif

/**
 * SECTION:ide-task
 * @title: IdeTask
 * @short_description: asynchronous task management
 *
 * #IdeTask is meant to be an improved form of #GTask. There are a few
 * deficiencies in #GTask that have made it unsuitable for certain use cases.
 *
 * #GTask does not provide a way to guarantee that the source object,
 * task data, and unused results are freed with in a given #GMainContext.
 * #IdeTask addresses this by having a more flexible result and object
 * ownership control.
 *
 * Furthermore, #IdeTask allows consumers to force disposal from a given
 * thread so that the data is released there.
 *
 * #IdeTask also supports chaining tasks together which makes it simpler
 * to avoid doing duplicate work by instead simply chaining the tasks.
 *
 * There are some costs to this design. It uses the main context a bit
 * more than #GTask may use it.
 *
 * #IdeTask allows setting a task kind which determines which thread pool
 * the task will be executed (and throttled) on.
 *
 * Because #IdeTask needs more control over result life-cycles (for chaining
 * results), additional return methods have been provided. Consumers should
 * use ide_task_return_boxed() when working with boxed types as it allows us
 * to copy the result to another task. Additionally, ide_task_return_object()
 * provides a simplified API over ide_task_return_pointer() which also allows
 * copying the result to chained tasks.
 */

typedef struct
{
  /*
   * The pointer we were provided.
   */
  gpointer data;

  /*
   * The destroy notify for @data. We should only call this from the
   * main context associated with the task.
   */
  GDestroyNotify data_destroy;
} IdeTaskData;

typedef enum
{
  IDE_TASK_RESULT_NONE,
  IDE_TASK_RESULT_CANCELLED,
  IDE_TASK_RESULT_BOOLEAN,
  IDE_TASK_RESULT_INT,
  IDE_TASK_RESULT_ERROR,
  IDE_TASK_RESULT_OBJECT,
  IDE_TASK_RESULT_BOXED,
  IDE_TASK_RESULT_POINTER,
} IdeTaskResultType;

typedef struct
{
  /*
   * The type of result stored in our union @u.
   */
  IdeTaskResultType type;

  /*
   * To ensure that we can pass ownership back to the main context
   * from our worker thread, we need to be able to stash the reference
   * here in our result. It is also convenient as we need access to it
   * from the main context callback anyway.
   */
  IdeTask *task;

  /*
   * Additionally, we need to allow passing our main context reference
   * back so that it cannot be finalized in our thread.
   */
  GMainContext *main_context;

  /*
   * Priority for our GSource attached to @main_context.
   */
  gint complete_priority;

  /*
   * The actual result information, broken down by result @type.
   */
  union {
    gboolean  v_bool;
    gssize    v_int;
    GError   *v_error;
    GObject  *v_object;
    struct {
      GType    type;
      gpointer pointer;
    } v_boxed;
    struct {
      gpointer       pointer;
      GDestroyNotify destroy;
    } v_pointer;
  } u;
} IdeTaskResult;

typedef struct
{
  IdeTask      *task;
  GMainContext *main_context;
  gint          priority;
} IdeTaskCancel;

struct _IdeTask
{
  GObject parent_instance;

  /*
   * @global_link is used to store a pointer to the task in the global
   * queue during the lifetime of the task. This is a debugging feature
   * so that we can dump the list of active tasks from the debugger.
   */
  GList global_link;

  /*
   * Controls access to our private data. We only access structure
   * data while holding this mutex to ensure that we have consistency
   * between threads which could be accessing internals.
   */
  GMutex mutex;

  /*
   * The source object for the GAsyncResult interface. If we have set
   * release_on_propagate, this will be released when the task propagate
   * function is called.
   */
  gpointer source_object;

  /*
   * The cancellable that we're monitoring for task cancellation.
   */
  GCancellable *cancellable;

  /*
   * If ide_task_set_return_on_cancel() has been set, then we might be
   * listening for changes. Handling this will queue a completion
   */
  gulong cancel_handler;

  /*
   * The callback to execute upon completion of the operation. It will
   * be called from @main_contect after the operation completes.
   */
  GAsyncReadyCallback callback;
  gpointer user_data;

  /*
   * The name for the task. This string is interned so you should not
   * use dynamic names. They are meant to simplify the process of
   * debugging what task failed.
   */
  const gchar *name;

  /*
   * The GMainContext that was the thread default when the task was
   * created. Most operations are proxied back to this context so that
   * the consumer does not need to worry about thread safety.
   */
  GMainContext *main_context;

  /*
   * The task data that has been set for the task. Task data is released
   * from a callback in the #GMainContext if changed outside the main
   * context.
   */
  IdeTaskData *task_data;

  /*
   * The result for the task. If release_on_propagate as set to %FALSE,
   * then this may be kept around so that ide_task_chain() can be used to
   * duplicate the result to another task. This is convenient when multiple
   * async funcs race to do some work, allowing just a single winner with all
   * the callers getting the same result.
   */
  IdeTaskResult *result;

  /*
   * ide_task_chain() allows us to propagate the result of this task to
   * another task (for a limited number of result types). This is the
   * list of those tasks.
   */
  GPtrArray *chained;

  /*
   * If ide_task_run_in_thread() is called, this will be set to the func
   * that should be called from within the thread.
   */
  IdeTaskThreadFunc thread_func;

  /*
   * If we're running in a thread, we'll stash the value here until we
   * can complete things cleanly and pass ownership back as one operation.
   */
  IdeTaskResult *thread_result;

  /*
   * The source tag for the task, which can be used to determine what
   * the task is from a debugger as well as to verify correctness
   * in async finish functions.
   */
  gpointer source_tag;

#ifdef ENABLE_TIME_CHART
  /* The time the task was created */
  gint64 begin_time;
#endif

  /*
   * Our priority for scheduling tasks in the particular workqueue.
   */
  gint priority;

  /*
   * The priority for completing the result back on the main context. This
   * defaults to a value lower than gtk redraw priority to ensure that gtk
   * has higher priority than task completion.
   */
  gint complete_priority;

  /*
   * While we're waiting for our return callback, this is set to our
   * source id. We use that to know we need to block on the main loop
   * in case the user calls ide_task_propagate_*() synchronously without
   * round-triping to the main loop.
   */
  guint return_source;

  /*
   * Our kind of task, which is used to determine what thread pool we
   * can use when running threaded work. This can be used to help choke
   * lots of work down to a relatively small number of threads.
   */
  IdeTaskKind kind : 8;

  /*
   * If the task has been completed, which is to say that the callback
   * dispatch has occurred in @main_context.
   */
  guint completed : 1;

  /*
   * If we should check @cancellable before returning the result. If set
   * to true, and the cancellable was cancelled, an error will be returned
   * even if the task completed successfully.
   */
  guint check_cancellable : 1;

  /*
   * If we should synthesize completion from a GCancellable::cancelled
   * event instead of waiting for the task to complete normally.
   */
  guint return_on_cancel : 1;

  /*
   * If we should release the source object and task data after we've
   * dispatched the callback (or the callback was NULL). This allows us
   * to ensure that various dependent data are released in the main
   * context. This is the default and helps ensure thread-safety.
   */
  guint release_on_propagate : 1;

  /*
   * Protect against multiple return calls, and given the developer a good
   * warning so they catch this early.
   */
  guint return_called : 1;

  /*
   * If we got a result that was a cancellation, then we mark it here so
   * that we can deal with it cleanly later.
   */
  guint got_cancel : 1;

  /*
   * If we have dispatched to a thread already.
   */
  guint thread_called : 1;
};

static void     async_result_init_iface (GAsyncResultIface *iface);
static void     ide_task_data_free      (IdeTaskData       *task_data);
static void     ide_task_result_free    (IdeTaskResult     *result);
static gboolean ide_task_return_cb      (gpointer           user_data);
static void     ide_task_release        (IdeTask           *self,
                                         gboolean           force);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeTaskData, ide_task_data_free);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeTaskResult, ide_task_result_free);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeTask, ide_task, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, async_result_init_iface))

enum {
  PROP_0,
  PROP_COMPLETED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static GQueue global_task_list = G_QUEUE_INIT;

G_LOCK_DEFINE (global_task_list);

static void
ide_task_cancel_free (IdeTaskCancel *cancel)
{
  g_clear_pointer (&cancel->main_context, g_main_context_unref);
  g_clear_object (&cancel->task);
  g_slice_free (IdeTaskCancel, cancel);
}

static const gchar *
result_type_name (IdeTaskResultType type)
{
  switch (type)
    {
    case IDE_TASK_RESULT_NONE:
      return "none";

    case IDE_TASK_RESULT_CANCELLED:
      return "cancelled";

    case IDE_TASK_RESULT_INT:
      return "int";

    case IDE_TASK_RESULT_POINTER:
      return "pointer";

    case IDE_TASK_RESULT_OBJECT:
      return "object";

    case IDE_TASK_RESULT_BOXED:
      return "boxed";

    case IDE_TASK_RESULT_BOOLEAN:
      return "boolean";

    case IDE_TASK_RESULT_ERROR:
      return "error";

    default:
      return NULL;
    }
}

static void
ide_task_data_free (IdeTaskData *task_data)
{
  if (task_data->data_destroy != NULL)
    task_data->data_destroy (task_data->data);
  g_slice_free (IdeTaskData, task_data);
}

static IdeTaskResult *
ide_task_result_copy (const IdeTaskResult *src)
{
  IdeTaskResult *dst;

  dst = g_slice_new0 (IdeTaskResult);
  dst->type = src->type;

  switch (src->type)
    {
    case IDE_TASK_RESULT_INT:
      dst->u.v_int = src->u.v_int;
      break;

    case IDE_TASK_RESULT_BOOLEAN:
      dst->u.v_bool = src->u.v_bool;
      break;

    case IDE_TASK_RESULT_ERROR:
      dst->u.v_error = g_error_copy (src->u.v_error);
      break;

    case IDE_TASK_RESULT_OBJECT:
      dst->u.v_object = src->u.v_object ? g_object_ref (src->u.v_object) : NULL;
      break;

    case IDE_TASK_RESULT_BOXED:
      dst->u.v_boxed.type = src->u.v_boxed.type;
      dst->u.v_boxed.pointer = g_boxed_copy (src->u.v_boxed.type, src->u.v_boxed.pointer);
      break;

    case IDE_TASK_RESULT_POINTER:
      g_critical ("Cannot proxy raw pointers for task results");
      break;

    case IDE_TASK_RESULT_CANCELLED:
    case IDE_TASK_RESULT_NONE:
    default:
      break;
    }

  return g_steal_pointer (&dst);
}

static void
ide_task_result_free (IdeTaskResult *result)
{
  if (result == NULL)
    return;

  switch (result->type)
    {
    case IDE_TASK_RESULT_POINTER:
      if (result->u.v_pointer.destroy)
        result->u.v_pointer.destroy (result->u.v_pointer.pointer);
      break;

    case IDE_TASK_RESULT_ERROR:
      g_error_free (result->u.v_error);
      break;

    case IDE_TASK_RESULT_BOXED:
      if (result->u.v_boxed.pointer)
        g_boxed_free (result->u.v_boxed.type, result->u.v_boxed.pointer);
      break;

    case IDE_TASK_RESULT_OBJECT:
      g_clear_object (&result->u.v_object);
      break;

    case IDE_TASK_RESULT_BOOLEAN:
    case IDE_TASK_RESULT_INT:
    case IDE_TASK_RESULT_NONE:
    case IDE_TASK_RESULT_CANCELLED:
    default:
      break;
    }

  g_clear_object (&result->task);
  g_clear_pointer (&result->main_context, g_main_context_unref);
  g_slice_free (IdeTaskResult, result);
}

/*
 * ide_task_complete:
 * @result: (transfer full): the result to complete
 *
 * queues the completion for the task. make sure that you've
 * set the result->task, main_context, and priority first.
 *
 * This is designed to allow stealing the last reference from
 * a worker thread and pass it back to the main context.
 *
 * Returns: a gsource identifier
 */
static guint
ide_task_complete (IdeTaskResult *result)
{
  GSource *source;
  guint ret;

  g_assert (result != NULL);
  g_assert (IDE_IS_TASK (result->task));
  g_assert (result->main_context);

  source = g_idle_source_new ();
  g_source_set_name (source, "[ide-task] complete result");
  g_source_set_ready_time (source, -1);
  g_source_set_callback (source, ide_task_return_cb, result, NULL);
  g_source_set_priority (source, result->complete_priority);
  ret = g_source_attach (source, result->main_context);
  g_source_unref (source);

  return ret;
}

static void
ide_task_thread_func (gpointer data)
{
  g_autoptr(GObject) source_object = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(IdeTask) task = data;
  IdeTask *self = task;
  gpointer task_data = NULL;
  IdeTaskThreadFunc thread_func;

  g_assert (IDE_IS_TASK (task));

  g_mutex_lock (&self->mutex);
  source_object = self->source_object ? g_object_ref (self->source_object) : NULL;
  cancellable = self->cancellable ? g_object_ref (self->cancellable) : NULL;
  if (self->task_data)
    task_data = self->task_data->data;
  thread_func = self->thread_func;
  self->thread_func = NULL;
  g_mutex_unlock (&self->mutex);

  g_assert (thread_func != NULL);

  thread_func (task, source_object, task_data, cancellable);

  g_clear_object (&source_object);
  g_clear_object (&cancellable);

  g_mutex_lock (&self->mutex);

  /*
   * We've delayed our ide_task_return() until we reach here, so now
   * we can steal our object instance and complete the task along with
   * ensuring the object wont be finalized from this thread.
   */
  if (self->thread_result)
    {
      IdeTaskResult *result = g_steal_pointer (&self->thread_result);

      g_assert (result->task == task);
      g_clear_object (&result->task);
      result->task = g_steal_pointer (&task);

      self->return_source = ide_task_complete (g_steal_pointer (&result));

      g_assert (source_object == NULL);
      g_assert (cancellable == NULL);
      g_assert (task == NULL);
    }
  else
    {
      /* The task did not return a value while in the thread func!  GTask
       * doesn't support this, but its useful to us in a number of ways, so
       * we'll begrudgingly support it but the best we can do is drop our
       * reference from the thread.
       */
    }

  g_mutex_unlock (&self->mutex);

  g_assert (source_object == NULL);
  g_assert (cancellable == NULL);
}

static void
ide_task_dispose (GObject *object)
{
  IdeTask *self = (IdeTask *)object;

  g_assert (IDE_IS_TASK (self));

  ide_task_release (self, TRUE);

  g_mutex_lock (&self->mutex);
  g_clear_pointer (&self->result, ide_task_result_free);
  g_mutex_unlock (&self->mutex);

  G_OBJECT_CLASS (ide_task_parent_class)->dispose (object);
}

static void
ide_task_finalize (GObject *object)
{
  IdeTask *self = (IdeTask *)object;

  G_LOCK (global_task_list);
  g_queue_unlink (&global_task_list, &self->global_link);
  G_UNLOCK (global_task_list);

  if (!self->return_called)
    g_critical ("%s [%s] finalized before completing",
                G_OBJECT_TYPE_NAME (self),
                self->name ?: "unnamed");
  else if (self->chained && self->chained->len)
    g_critical ("%s [%s] finalized before dependents were notified",
                G_OBJECT_TYPE_NAME (self),
                self->name ?: "unnamed");
  else if (self->thread_func)
    g_critical ("%s [%s] finalized while thread_func is active",
                G_OBJECT_TYPE_NAME (self),
                self->name ?: "unnamed");
  else if (!self->completed)
    g_critical ("%s [%s] finalized before completion",
                G_OBJECT_TYPE_NAME (self),
                self->name ?: "unnamed");

  g_assert (self->return_source == 0);
  g_assert (self->result == NULL);
  g_assert (self->task_data == NULL);
  g_assert (self->source_object == NULL);
  g_assert (self->chained == NULL);
  g_assert (self->thread_result == NULL);

  g_clear_pointer (&self->main_context, g_main_context_unref);
  g_clear_object (&self->cancellable);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (ide_task_parent_class)->finalize (object);
}

static void
ide_task_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeTask *self = IDE_TASK (object);

  switch (prop_id)
    {
    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_task_get_completed (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_task_class_init (IdeTaskClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_task_dispose;
  object_class->finalize = ide_task_finalize;
  object_class->get_property = ide_task_get_property;

  properties [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          "Completed",
                          "If the task has completed",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /* This can be called multiple times, so we use this to allow
   * unit tests to work without having to expose the function as
   * public API.
   */
  _ide_thread_pool_init (FALSE);
}

static void
ide_task_init (IdeTask *self)
{
  g_mutex_init (&self->mutex);

  self->check_cancellable = TRUE;
  self->release_on_propagate = TRUE;
  self->priority = G_PRIORITY_DEFAULT;
  self->complete_priority = PRIORITY_REDRAW + 1;
  self->main_context = g_main_context_ref_thread_default ();
  self->global_link.data = self;

  G_LOCK (global_task_list);
  g_queue_push_tail_link (&global_task_list, &self->global_link);
  G_UNLOCK (global_task_list);
}

/**
 * ide_task_get_source_object: (skip)
 * @self: a #IdeTask
 *
 * Gets the #GObject used when creating the source object.
 *
 * As this does not provide ownership transfer of the #GObject, it is a
 * programmer error to call this function outside of a thread worker called
 * from ide_task_run_in_thread() or outside the #GMainContext that is
 * associated with the task.
 *
 * If you need to access the object in other scenarios, you must use the
 * g_async_result_get_source_object() which provides a full reference to the
 * source object, safely. You are responsible for ensuring that you do not
 * release the object in a manner that is unsafe for the source object.
 *
 * Returns: (transfer none) (nullable) (type GObject.Object): a #GObject or %NULL
 */
gpointer
ide_task_get_source_object (IdeTask *self)
{
  gpointer ret;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = self->source_object;
  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_task_new:
 * @source_object: (type GObject.Object) (nullable): a #GObject or %NULL
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (scope async) (nullable): a #GAsyncReadyCallback or %NULL
 * @user_data: closure data for @callback
 *
 * Creates a new #IdeTask.
 *
 * #IdeTask is similar to #GTask but provides some additional guarantees
 * such that by default, the source object, task data, and unused results
 * are guaranteed to be finalized in the #GMainContext associated with
 * the task itself.
 *
 * Returns: (transfer full): an #IdeTask
 */
IdeTask *
(ide_task_new) (gpointer             source_object,
                GCancellable        *cancellable,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  g_autoptr(IdeTask) self = NULL;

  g_return_val_if_fail (!source_object || G_IS_OBJECT (source_object), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  self = g_object_new (IDE_TYPE_TASK, NULL);

  self->source_object = source_object ? g_object_ref (source_object) : NULL;
  self->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  self->callback = callback;
  self->user_data = user_data;
#ifdef ENABLE_TIME_CHART
  self->begin_time = g_get_monotonic_time ();
#endif

  return g_steal_pointer (&self);
}

/**
 * ide_task_is_valid:
 * @self: (nullable) (type IdeTask): a #IdeTask
 * @source_object: (nullable): a #GObject or %NULL
 *
 * Checks if @source_object matches the object the task was created with.
 *
 * Returns: %TRUE is source_object matches
 */
gboolean
ide_task_is_valid (gpointer self,
                   gpointer source_object)
{
  return IDE_IS_TASK (self) && IDE_TASK (self)->source_object == source_object;
}

/**
 * ide_task_get_completed:
 * @self: a #IdeTask
 *
 * Gets the "completed" property. This is %TRUE after the callback used when
 * creating the task has been executed.
 *
 * The property will be notified using g_object_notify() exactly once in the
 * same #GMainContext as the callback.
 *
 * Returns: %TRUE if the task has completed
 */
gboolean
ide_task_get_completed (IdeTask *self)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_TASK (self), FALSE);

  g_mutex_lock (&self->mutex);
  ret = self->completed;
  g_mutex_unlock (&self->mutex);

  return ret;
}

gint
ide_task_get_priority (IdeTask *self)
{
  gint ret;

  g_return_val_if_fail (IDE_IS_TASK (self), 0);

  g_mutex_lock (&self->mutex);
  ret = self->priority;
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
ide_task_set_priority (IdeTask *self,
                       gint     priority)
{
  g_return_if_fail (IDE_IS_TASK (self));

  g_mutex_lock (&self->mutex);
  self->priority = priority;
  g_mutex_unlock (&self->mutex);
}

gint
ide_task_get_complete_priority (IdeTask *self)
{
  gint ret;

  g_return_val_if_fail (IDE_IS_TASK (self), 0);

  g_mutex_lock (&self->mutex);
  ret = self->complete_priority;
  g_mutex_unlock (&self->mutex);

  return ret;
}

void
ide_task_set_complete_priority (IdeTask *self,
                                gint     complete_priority)
{
  g_return_if_fail (IDE_IS_TASK (self));

  g_mutex_lock (&self->mutex);
  self->complete_priority = complete_priority;
  g_mutex_unlock (&self->mutex);
}

/**
 * ide_task_get_cancellable:
 * @self: a #IdeTask
 *
 * Gets the #GCancellable for the task.
 *
 * Returns: (transfer none) (nullable): a #GCancellable or %NULL
 */
GCancellable *
ide_task_get_cancellable (IdeTask *self)
{
  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  return self->cancellable;
}

static void
ide_task_deliver_result (IdeTask       *self,
                         IdeTaskResult *result)
{
  g_assert (IDE_IS_TASK (self));
  g_assert (result != NULL);
  g_assert (result->task == NULL);
  g_assert (result->main_context == NULL);

  /* This task was chained from another task. This completes the result
   * and we should dispatch the callback. To simplify the dispatching and
   * help prevent any re-entrancy issues, we defer back to the main context
   * to complete the operation.
   */

  result->task = g_object_ref (self);
  result->main_context = g_main_context_ref (self->main_context);
  result->complete_priority = self->complete_priority;

  g_mutex_lock (&self->mutex);

  self->return_called = TRUE;
  self->return_source = ide_task_complete (g_steal_pointer (&result));

  g_mutex_unlock (&self->mutex);
}

static void
ide_task_release (IdeTask  *self,
                  gboolean  force)
{
  g_autoptr(IdeTaskData) task_data = NULL;
  g_autoptr(GObject) source_object = NULL;
  g_autoptr(GPtrArray) chained = NULL;

  g_assert (IDE_IS_TASK (self));

  g_mutex_lock (&self->mutex);
  if (force || self->release_on_propagate)
    {
      source_object = g_steal_pointer (&self->source_object);
      task_data = g_steal_pointer (&self->task_data);
      chained = g_steal_pointer (&self->chained);
    }
  g_mutex_unlock (&self->mutex);

  if (chained)
    {
      for (guint i = 0; i < chained->len; i++)
        {
          IdeTask *task = g_ptr_array_index (chained, i);

          ide_task_return_new_error (task,
                                     G_IO_ERROR,
                                     G_IO_ERROR_FAILED,
                                     "Error synthesized for task, parent task disposed");
        }
    }
}

static gboolean
ide_task_return_cb (gpointer user_data)
{
  g_autoptr(IdeTask) self = NULL;
  g_autoptr(IdeTaskResult) result = user_data;
  g_autoptr(IdeTaskResult) result_copy = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(GObject) source_object = NULL;
  g_autoptr(GPtrArray) chained = NULL;
  GAsyncReadyCallback callback = NULL;
  gpointer callback_data = NULL;

  g_assert (result != NULL);
  g_assert (IDE_IS_TASK (result->task));

  /* We steal the task object, because we only stash it in the result
   * structure to get it here. And if we held onto it, we would have
   * a reference cycle.
   */
  self = g_steal_pointer (&result->task);

#ifdef ENABLE_TIME_CHART
  g_message ("TASK-END: %s: duration=%lf",
             self->name,
             (g_get_monotonic_time () - self->begin_time) / (gdouble)G_USEC_PER_SEC);
#endif

  g_mutex_lock (&self->mutex);

  g_assert (self->return_source != 0);

  self->return_source = 0;

  if (self->got_cancel && self->result != NULL)
    {
      /* We can discard this since we already handled a result for the
       * task. We delivered this here just so that we could finalize
       * any objects back inside them main context.
       */
      g_mutex_unlock (&self->mutex);
      return G_SOURCE_REMOVE;
    }

  g_assert (self->result == NULL);
  g_assert (self->return_called == TRUE);

  self->result = g_steal_pointer (&result);

  callback = self->callback;
  callback_data = self->user_data;

  self->callback = NULL;
  self->user_data = NULL;

  source_object = self->source_object ? g_object_ref (self->source_object) : NULL;
  cancellable = self->cancellable ? g_object_ref (self->cancellable) : NULL;

  chained = g_steal_pointer (&self->chained);

  /* Make a private copy of the result data if we're going to need to notify
   * other tasks of our result. We can't guarantee the result in @task will
   * stay alive during our dispatch callbacks, so we need to have a copy.
   */
  if (chained != NULL && chained->len > 0)
    result_copy = ide_task_result_copy (self->result);

  g_mutex_unlock (&self->mutex);

  if (callback)
    callback (source_object, G_ASYNC_RESULT (self), callback_data);

  if (chained)
    {
      for (guint i = 0; i < chained->len; i++)
        {
          IdeTask *other = g_ptr_array_index (chained, i);
          g_autoptr(IdeTaskResult) other_result = ide_task_result_copy (result_copy);

          ide_task_deliver_result (other, g_steal_pointer (&other_result));
        }
    }

  g_mutex_lock (&self->mutex);
  self->completed = TRUE;
  g_mutex_unlock (&self->mutex);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMPLETED]);

  ide_task_release (self, FALSE);

  return G_SOURCE_REMOVE;
}

static gboolean
ide_task_return_dummy_cb (gpointer data)
{
  return G_SOURCE_REMOVE;
}

static void
ide_task_return (IdeTask       *self,
                 IdeTaskResult *result)
{
  g_autoptr(GMutexLocker) locker = NULL;

  g_assert (IDE_IS_TASK (self));
  g_assert (result != NULL);
  g_assert (result->task == NULL);

  locker = g_mutex_locker_new (&self->mutex);

  if (self->cancel_handler && self->cancellable)
    {
      g_cancellable_disconnect (self->cancellable, self->cancel_handler);
      self->cancel_handler = 0;
    }

  if (self->return_called)
    {
      GSource *source;

      if (result->type == IDE_TASK_RESULT_CANCELLED)
        {
          /* We already had a result, and now raced to be notified of
           * cancellation. We can safely free this result even if we're
           * currently in a worker thread.
           */
          ide_task_result_free (result);
          return;
        }

      /* If we haven't been cancelled, then we reached this path multiple
       * times by programmer error.
       */
      if (!self->got_cancel)
        g_critical ("Attempted to set result on task [%s] multiple times", self->name);

      /*
       * This task has already returned, but we need to ensure that we pass
       * the data back to the main context so that it is freed appropriately.
       */

      source = g_idle_source_new ();
      g_source_set_name (source, "[ide-task] finalize task result");
      g_source_set_ready_time (source, -1);
      g_source_set_callback (source,
                             ide_task_return_dummy_cb,
                             result,
                             (GDestroyNotify)ide_task_result_free);
      g_source_attach (source, self->main_context);
      g_source_unref (source);

      return;
    }

  self->return_called = TRUE;

  if (result->type == IDE_TASK_RESULT_CANCELLED)
    self->got_cancel = TRUE;

  result->task = g_object_ref (self);
  result->main_context = g_main_context_ref (self->main_context);
  result->complete_priority = self->complete_priority;

  /* We can queue the result immediately if we're not being called
   * while we're inside of a ide_task_run_in_thread() callback. Otherwise,
   * that thread cleanup must complete this to ensure objects cannot
   * be finalized in that thread.
   */
  if (!self->thread_called || IDE_IS_MAIN_THREAD ())
    self->return_source = ide_task_complete (result);
  else if (self->return_on_cancel && result->type == IDE_TASK_RESULT_CANCELLED)
    self->return_source = ide_task_complete (result);
  else
    self->thread_result = result;
}

/**
 * ide_task_return_int:
 * @self: a #IdeTask
 * @result: the result for the task
 *
 * Sets the result of the task to @result.
 *
 * Other tasks depending on the result will be notified after returning
 * to the #GMainContext of the task.
 */
void
ide_task_return_int (IdeTask *self,
                     gssize   result)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_INT;
  ret->u.v_int = result;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_boolean:
 * @self: a #IdeTask
 * @result: the result for the task
 *
 * Sets the result of the task to @result.
 *
 * Other tasks depending on the result will be notified after returning
 * to the #GMainContext of the task.
 */
void
ide_task_return_boolean (IdeTask  *self,
                         gboolean  result)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_BOOLEAN;
  ret->u.v_bool = !!result;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_boxed: (skip)
 * @self: a #IdeTask
 * @result_type: the #GType of the boxed type
 * @result: (transfer full): the result to be returned
 *
 * This is similar to ide_task_return_pointer(), but allows the task to
 * know the boxed #GType so that the result may be propagated to chained
 * tasks.
 */
void
ide_task_return_boxed (IdeTask  *self,
                       GType     result_type,
                       gpointer  result)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));
  g_return_if_fail (result_type != G_TYPE_INVALID);
  g_return_if_fail (G_TYPE_IS_BOXED (result_type));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_BOXED;
  ret->u.v_boxed.type = result_type;
  ret->u.v_boxed.pointer = result;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_object:
 * @self: a #IdeTask
 * @instance: (transfer full) (type GObject.Object): a #GObject instance
 *
 * Returns a new object instance.
 *
 * Takes ownership of @instance to allow saving a reference increment and
 * decrement by the caller.
 */
void
ide_task_return_object (IdeTask  *self,
                        gpointer  instance)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));
  g_return_if_fail (!instance || G_IS_OBJECT (instance));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_OBJECT;
  ret->u.v_object = instance;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_pointer: (skip)
 * @self: a #IdeTask
 * @data: the data to return
 * @destroy: an optional #GDestroyNotify to cleanup data if no handler
 *   propagates the result
 *
 * Returns a new raw pointer.
 *
 * Note that pointers cannot be chained to other tasks, so you may not
 * use ide_task_chain() in conjunction with a task returning a pointer
 * using ide_task_return_pointer().
 *
 * If you need task chaining with pointers, see ide_task_return_boxed()
 * or ide_task_return_object().
 */
void
(ide_task_return_pointer) (IdeTask        *self,
                           gpointer        data,
                           GDestroyNotify  destroy)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_POINTER;
  ret->u.v_pointer.pointer = data;
  ret->u.v_pointer.destroy = destroy;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_error:
 * @self: a #IdeTask
 * @error: (transfer full): a #GError
 *
 * Sets @error as the result of the #IdeTask
 */
void
ide_task_return_error (IdeTask *self,
                       GError  *error)
{
  IdeTaskResult *ret;

  g_return_if_fail (IDE_IS_TASK (self));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_ERROR;
  ret->u.v_error = error;

  ide_task_return (self, g_steal_pointer (&ret));
}

/**
 * ide_task_return_new_error:
 * @self: a #IdeTask
 * @error_domain: the error domain of the #GError
 * @error_code: the error code for the #GError
 * @format: the printf-style format string
 *
 * Creates a new #GError and sets it as the result for the task.
 */
void
ide_task_return_new_error (IdeTask     *self,
                           GQuark       error_domain,
                           gint         error_code,
                           const gchar *format,
                           ...)
{
  GError *error;
  va_list args;

  g_return_if_fail (IDE_IS_TASK (self));

  va_start (args, format);
  error = g_error_new_valist (error_domain, error_code, format, args);
  va_end (args);

  ide_task_return_error (self, g_steal_pointer (&error));
}

/**
 * ide_task_return_error_if_cancelled:
 * @self: a #IdeTask
 *
 * Returns a new #GError if the cancellable associated with the task
 * has been cancelled. If so, %TRUE is returned, otherwise %FALSE.
 *
 * If the source object related to the task is an #IdeObject and that
 * object has had been requested to destroy, it too will be considered
 * a cancellation state.
 *
 * Returns: %TRUE if the task was cancelled and error returned.
 */
gboolean
ide_task_return_error_if_cancelled (IdeTask *self)
{
  GError *error = NULL;
  gboolean failed;

  g_return_val_if_fail (IDE_IS_TASK (self), FALSE);

  g_mutex_lock (&self->mutex);
  failed = g_cancellable_is_cancelled (self->cancellable) ||
    (IDE_IS_OBJECT (self->source_object) &&
     !ide_object_check_ready (IDE_OBJECT (self->source_object), &error));
  g_mutex_unlock (&self->mutex);

  if (failed)
    {
      if (error != NULL)
        ide_task_return_error (self, g_steal_pointer (&error));
      else
        ide_task_return_new_error (self,
                                   G_IO_ERROR,
                                   G_IO_ERROR_CANCELLED,
                                   "The task was cancelled");
    }

  return failed;
}

/**
 * ide_task_set_release_on_propagate:
 * @self: a #IdeTask
 * @release_on_propagate: if data should be released on propagate
 *
 * Setting this to %TRUE (the default) ensures that the task will release all
 * task data and source_object references after executing the configured
 * callback. This is useful to ensure that dependent objects are finalized
 * in the thread-default #GMainContext the task was created in.
 *
 * Generally, you want to leave this as %TRUE to ensure thread-safety on the
 * dependent objects and task data.
 */
void
ide_task_set_release_on_propagate (IdeTask  *self,
                                   gboolean  release_on_propagate)
{
  g_return_if_fail (IDE_IS_TASK (self));

  release_on_propagate = !!release_on_propagate;

  g_mutex_lock (&self->mutex);
  self->release_on_propagate = release_on_propagate;
  g_mutex_unlock (&self->mutex);
}

/**
 * ide_task_set_source_tag:
 * @self: a #IdeTask
 * @source_tag: a tag to identify the task, usual a function pointer
 *
 * Sets the source tag for the task. Generally this is a function pointer
 * of the function that created the task.
 */
void
ide_task_set_source_tag (IdeTask  *self,
                         gpointer  source_tag)
{
  g_return_if_fail (IDE_IS_TASK (self));

  g_mutex_lock (&self->mutex);
  self->source_tag = source_tag;
  g_mutex_unlock (&self->mutex);
}

/**
 * ide_task_set_check_cancellable:
 * @self: a #IdeTask
 * @check_cancellable: %TRUE if the cancellable should be checked
 *
 * Setting @check_cancellable to %TRUE (the default) ensures that the
 * #GCancellable used when creating the #IdeTask is checked for cancellation
 * before propagating a result. If cancelled, an error will be returned
 * instead of the result.
 */
void
ide_task_set_check_cancellable (IdeTask  *self,
                                gboolean  check_cancellable)
{
  g_return_if_fail (IDE_IS_TASK (self));

  check_cancellable = !!check_cancellable;

  g_mutex_lock (&self->mutex);
  self->check_cancellable = check_cancellable;
  g_mutex_unlock (&self->mutex);
}

/**
 * ide_task_run_in_thread: (skip)
 * @self: a #IdeTask
 * @thread_func: a function to execute on a worker thread
 *
 * Scheules @thread_func to be executed on a worker thread.
 *
 * @thread_func must complete the task from the worker thread using one of
 * ide_task_return_boolean(), ide_task_return_int(), or
 * ide_task_return_pointer().
 */
void
ide_task_run_in_thread (IdeTask           *self,
                        IdeTaskThreadFunc  thread_func)
{
  g_autoptr(GError) error = NULL;

  g_return_if_fail (IDE_IS_TASK (self));
  g_return_if_fail (thread_func != NULL);

  g_mutex_lock (&self->mutex);

  if (self->completed == TRUE)
    {
      g_critical ("Task already completed, cannot run in thread");
      goto unlock;
    }

  if (self->thread_called)
    {
      g_critical ("Run in thread already called, cannot run again");
      goto unlock;
    }

  self->thread_called = TRUE;
  self->thread_func = thread_func;

  ide_thread_pool_push_with_priority ((IdeThreadPoolKind)self->kind,
                                      self->priority,
                                      ide_task_thread_func,
                                      g_object_ref (self));

unlock:
  g_mutex_unlock (&self->mutex);

  if (error != NULL)
    ide_task_return_error (self, g_steal_pointer (&error));
}

static IdeTaskResult *
ide_task_propagate_locked (IdeTask            *self,
                           IdeTaskResultType   expected_type,
                           GError            **error)
{
  IdeTaskResult *ret = NULL;

  g_assert (IDE_IS_TASK (self));
  g_assert (expected_type > IDE_TASK_RESULT_NONE);

  if (self->result == NULL)
    {
      g_autoptr(GMainContext) context = g_main_context_ref (self->main_context);

      while (self->return_source)
        {
          g_mutex_unlock (&self->mutex);
          g_main_context_iteration (context, FALSE);
          g_mutex_lock (&self->mutex);
        }
    }

  if (self->result == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "No result available for task");
    }
  else if (self->result->type == IDE_TASK_RESULT_ERROR)
    {
      if (error != NULL)
        *error = g_error_copy (self->result->u.v_error);
    }
  else if ((self->check_cancellable && g_cancellable_is_cancelled (self->cancellable)) ||
           self->result->type == IDE_TASK_RESULT_CANCELLED)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "The operation was cancelled");
    }
  else if (IDE_IS_OBJECT (self->source_object) &&
           ide_object_in_destruction (IDE_OBJECT (self->source_object)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CANCELLED,
                   "The object was destroyed while the task executed");
    }
  else if (self->result->type != expected_type)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Task expected result of %s got %s",
                   result_type_name (expected_type),
                   result_type_name (self->result->type));
    }
  else
    {
      g_assert (self->result != NULL);
      g_assert (self->result->type == expected_type);

      if (self->release_on_propagate)
        ret = g_steal_pointer (&self->result);
      else if (self->result->type == IDE_TASK_RESULT_POINTER)
        ret = g_steal_pointer (&self->result);
      else
        ret = ide_task_result_copy (self->result);

      g_assert (ret != NULL);
    }

  return ret;
}

gboolean
ide_task_propagate_boolean (IdeTask  *self,
                            GError  **error)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTaskResult) res = NULL;

  g_return_val_if_fail (IDE_IS_TASK (self), FALSE);

  locker = g_mutex_locker_new (&self->mutex);

  if (!(res = ide_task_propagate_locked (self, IDE_TASK_RESULT_BOOLEAN, error)))
    return FALSE;

  return res->u.v_bool;
}

gpointer
ide_task_propagate_boxed (IdeTask  *self,
                          GError  **error)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTaskResult) res = NULL;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  locker = g_mutex_locker_new (&self->mutex);

  if (!(res = ide_task_propagate_locked (self, IDE_TASK_RESULT_BOXED, error)))
    return NULL;

  return g_boxed_copy (res->u.v_boxed.type, res->u.v_boxed.pointer);
}

gssize
ide_task_propagate_int (IdeTask  *self,
                        GError  **error)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTaskResult) res = NULL;

  g_return_val_if_fail (IDE_IS_TASK (self), 0);

  locker = g_mutex_locker_new (&self->mutex);

  if (!(res = ide_task_propagate_locked (self, IDE_TASK_RESULT_INT, error)))
    return 0;

  return res->u.v_int;
}

/**
 * ide_task_propagate_object:
 * @self: a #IdeTask
 * @error: a location for a #GError, or %NULL
 *
 * Returns an object if the task completed with an object. Otherwise, %NULL
 * is returned.
 *
 * @error is set if the task completed with an error.
 *
 * Returns: (transfer full) (type GObject.Object): a #GObject or %NULL
 *   and @error may be set.
 */
gpointer
ide_task_propagate_object (IdeTask  *self,
                           GError  **error)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTaskResult) res = NULL;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  locker = g_mutex_locker_new (&self->mutex);

  if (!(res = ide_task_propagate_locked (self, IDE_TASK_RESULT_OBJECT, error)))
    return NULL;

  return g_steal_pointer (&res->u.v_object);
}

gpointer
ide_task_propagate_pointer (IdeTask  *self,
                            GError  **error)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTaskResult) res = NULL;
  gpointer ret;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  locker = g_mutex_locker_new (&self->mutex);

  if (!(res = ide_task_propagate_locked (self, IDE_TASK_RESULT_POINTER, error)))
    return NULL;

  ret = g_steal_pointer (&res->u.v_pointer.pointer);
  res->u.v_pointer.destroy = NULL;

  return ret;
}

/**
 * ide_task_chain:
 * @self: a #IdeTask
 * @other_task: a #IdeTask
 *
 * Causes the result of @self to also be delivered to @other_task.
 *
 * This API is useful in situations when you want to avoid doing the same
 * work multiple times, and can share the result between mutliple async
 * operations requesting the same work.
 *
 * Users of this API must make sure one of two things is true. Either they
 * have called ide_task_set_release_on_propagate() with @self and set
 * release_on_propagate to %FALSE, or @self has not yet completed.
 */
void
ide_task_chain (IdeTask *self,
                IdeTask *other_task)
{
  g_return_if_fail (IDE_IS_TASK (self));
  g_return_if_fail (IDE_IS_TASK (other_task));
  g_return_if_fail (self != other_task);

  g_mutex_lock (&self->mutex);

  if (self->result)
    {
      IdeTaskResult *copy = ide_task_result_copy (self->result);

      if (copy != NULL)
        ide_task_deliver_result (other_task, g_steal_pointer (&copy));
      else
        ide_task_return_new_error (other_task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "Result could not be copied to task");
    }
  else
    {
      if (self->chained == NULL)
        self->chained = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_add (self->chained, g_object_ref (other_task));
    }

  g_mutex_unlock (&self->mutex);
}

gpointer
ide_task_get_source_tag (IdeTask *self)
{
  gpointer ret;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = self->source_tag;
  g_mutex_unlock (&self->mutex);

  return ret;
}

IdeTaskKind
ide_task_get_kind (IdeTask *self)
{
  IdeTaskKind kind;

  g_return_val_if_fail (IDE_IS_TASK (self), 0);

  g_mutex_lock (&self->mutex);
  kind = self->kind;
  g_mutex_unlock (&self->mutex);

  return kind;
}

void
ide_task_set_kind (IdeTask     *self,
                   IdeTaskKind  kind)
{
  g_return_if_fail (IDE_IS_TASK (self));
  g_return_if_fail (kind >= IDE_TASK_KIND_DEFAULT);
  g_return_if_fail (kind < IDE_TASK_KIND_LAST);

  g_mutex_lock (&self->mutex);
  self->kind = kind;
  g_mutex_unlock (&self->mutex);
}

/**
 * ide_task_get_task_data: (skip)
 * @self: a #IdeTask
 *
 * Gets the task data previously set with ide_task_set_task_data().
 *
 * Returns: (transfer none): previously registered task data or %NULL
 */
gpointer
ide_task_get_task_data (IdeTask *self)
{
  gpointer task_data = NULL;

  g_assert (IDE_IS_TASK (self));

  g_mutex_lock (&self->mutex);
  if (self->task_data)
    task_data = self->task_data->data;
  g_mutex_unlock (&self->mutex);

  return task_data;
}

static gboolean
ide_task_set_task_data_cb (gpointer data)
{
  IdeTaskData *task_data = data;
  ide_task_data_free (task_data);
  return G_SOURCE_REMOVE;
}

void
(ide_task_set_task_data) (IdeTask        *self,
                          gpointer        task_data,
                          GDestroyNotify  task_data_destroy)
{
  g_autoptr(IdeTaskData) old_task_data = NULL;
  g_autoptr(IdeTaskData) new_task_data = NULL;

  g_return_if_fail (IDE_IS_TASK (self));

  new_task_data = g_slice_new0 (IdeTaskData);
  new_task_data->data = task_data;
  new_task_data->data_destroy = task_data_destroy;

  g_mutex_lock (&self->mutex);

  if (self->return_called)
    {
      g_critical ("Cannot set task data after returning value");
      goto unlock;
    }

  old_task_data = g_steal_pointer (&self->task_data);
  self->task_data = g_steal_pointer (&new_task_data);

  if (self->thread_called && old_task_data)
    {
      GSource *source;

      source = g_idle_source_new ();
      g_source_set_name (source, "[ide-task] finalize task data");
      g_source_set_ready_time (source, -1);
      g_source_set_callback (source,
                             ide_task_set_task_data_cb,
                             NULL, NULL);
      g_source_set_priority (source, self->priority);
      g_source_attach (source, self->main_context);
      g_source_unref (source);
    }

unlock:
  g_mutex_unlock (&self->mutex);
}

static gboolean
ide_task_cancel_cb (gpointer user_data)
{
  IdeTask *self = user_data;
  IdeTaskResult *ret;

  g_assert (IDE_IS_TASK (self));

  ret = g_slice_new0 (IdeTaskResult);
  ret->type = IDE_TASK_RESULT_CANCELLED;

  ide_task_return (self, g_steal_pointer (&ret));

  return G_SOURCE_REMOVE;
}

static void
ide_task_cancellable_cancelled_cb (GCancellable  *cancellable,
                                   IdeTaskCancel *cancel)
{
  GSource *source;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (cancel != NULL);
  g_assert (IDE_IS_TASK (cancel->task));
  g_assert (cancel->main_context != NULL);

  /*
   * This can be called synchronously from g_cancellable_connect(), which
   * could still be holding self->mutex. So we need to queue the cancellation
   * request back through the main context.
   */

  source = g_idle_source_new ();
  g_source_set_name (source, "[ide-task] cancel task");
  g_source_set_ready_time (source, -1);
  g_source_set_callback (source, ide_task_cancel_cb, g_object_ref (cancel->task), g_object_unref);
  g_source_set_priority (source, cancel->priority);
  g_source_attach (source, cancel->main_context);
  g_source_unref (source);
}

/**
 * ide_task_get_return_on_cancel:
 * @self: a #IdeTask
 *
 * Gets the return_on_cancel value, which means the task will return
 * immediately when the #GCancellable is cancelled.
 */
gboolean
ide_task_get_return_on_cancel (IdeTask *self)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_TASK (self), FALSE);

  g_mutex_lock (&self->mutex);
  ret = self->return_on_cancel;
  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_task_set_return_on_cancel:
 * @self: a #IdeTask
 * @return_on_cancel: if the task should return immediately when the
 *   #GCancellable has been cancelled.
 *
 * Setting @return_on_cancel to %TRUE ensures that the task will cancel
 * immediately when #GCancellable::cancelled is emitted by the configured
 * cancellable.
 *
 * Setting this requires that the caller can ensure the configured #GMainContext
 * will outlive the threaded worker so that task state can be freed in a delayed
 * fashion.
 */
void
ide_task_set_return_on_cancel (IdeTask  *self,
                               gboolean  return_on_cancel)
{
  g_autoptr(GMutexLocker) locker = NULL;

  g_return_if_fail (IDE_IS_TASK (self));

  locker = g_mutex_locker_new (&self->mutex);

  if (self->cancellable == NULL)
    return;

  return_on_cancel = !!return_on_cancel;

  if (self->return_on_cancel != return_on_cancel)
    {
      self->return_on_cancel = return_on_cancel;

      if (return_on_cancel)
        {
          IdeTaskCancel *cancel;

          /* This creates a reference cycle, but it gets destroyed when the
           * appropriate ide_task_return() API is called.
           */
          cancel = g_slice_new0 (IdeTaskCancel);
          cancel->main_context = g_main_context_ref (self->main_context);
          cancel->task = g_object_ref (self);
          cancel->priority = self->priority;

          self->cancel_handler =
            g_cancellable_connect (self->cancellable,
                                   G_CALLBACK (ide_task_cancellable_cancelled_cb),
                                   g_steal_pointer (&cancel),
                                   (GDestroyNotify)ide_task_cancel_free);

        }
      else
        {
          if (self->cancel_handler)
            {
              g_cancellable_disconnect (self->cancellable, self->cancel_handler);
              self->cancel_handler = 0;
            }
        }
    }
}

void
ide_task_report_new_error (gpointer              source_object,
                           GAsyncReadyCallback   callback,
                           gpointer              callback_data,
                           gpointer              source_tag,
                           GQuark                domain,
                           gint                  code,
                           const gchar          *format,
                           ...)
{
  g_autoptr(IdeTask) task = NULL;
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (domain, code, format, args);
  va_end (args);

  task = ide_task_new (source_object, NULL, callback, callback_data);
  ide_task_set_source_tag (task, source_tag);
  ide_task_return_error (task, g_steal_pointer (&error));
}

/**
 * ide_task_get_name:
 * @self: a #IdeTask
 *
 * Gets the name assigned for the task.
 *
 * Returns: (nullable): a string or %NULL
 */
const gchar *
ide_task_get_name (IdeTask *self)
{
  const gchar *ret;

  g_return_val_if_fail (IDE_IS_TASK (self), NULL);

  g_mutex_lock (&self->mutex);
  ret = self->name;
  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_task_set_name:
 * @self: a #IdeTask
 *
 * Sets a useful name for the task.
 *
 * This string is interned, so it is best to avoid dynamic names as
 * that can result in lots of unnecessary strings being interned for
 * the lifetime of the process.
 *
 * This name may be used in various g_critical() messages which can
 * be useful in troubleshooting.
 *
 * If using #IdeTask from C, a default name is set using the source
 * file name and line number.
 */
void
ide_task_set_name (IdeTask *self,
                   const gchar *name)
{
  g_return_if_fail (IDE_IS_TASK (self));

  name = g_intern_string (name);

  g_mutex_lock (&self->mutex);
  self->name = name;
  g_mutex_unlock (&self->mutex);

#ifdef ENABLE_TIME_CHART
  g_message ("TASK-BEGIN: %s", name);
#endif
}

/**
 * ide_task_had_error:
 * @self: a #IdeTask
 *
 * Checks to see if the task had an error.
 *
 * Returns: %TRUE if an error has occurred
 */
gboolean
ide_task_had_error (IdeTask *self)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_TASK (self), FALSE);

  g_mutex_lock (&self->mutex);
  ret = (self->result != NULL && self->result->type == IDE_TASK_RESULT_ERROR) ||
        (self->thread_result != NULL && self->thread_result->type == IDE_TASK_RESULT_ERROR);
  g_mutex_unlock (&self->mutex);

  return ret;
}

static gpointer
ide_task_get_user_data (GAsyncResult *result)
{
  IdeTask *self = (IdeTask *)result;

  g_assert (IDE_IS_TASK (self));

  return self->user_data;
}

static GObject *
ide_task_get_source_object_full (GAsyncResult *result)
{
  IdeTask *self = (IdeTask *)result;

  g_assert (IDE_IS_TASK (self));

  return self->source_object ? g_object_ref (self->source_object) : NULL;
}

static gboolean
ide_task_is_tagged (GAsyncResult *result,
                    gpointer      source_tag)
{
  IdeTask *self = (IdeTask *)result;

  g_assert (IDE_IS_TASK (self));

  return source_tag == self->source_tag;
}

static void
async_result_init_iface (GAsyncResultIface *iface)
{
  iface->get_user_data = ide_task_get_user_data;
  iface->get_source_object = ide_task_get_source_object_full;
  iface->is_tagged = ide_task_is_tagged;
}

void
_ide_dump_tasks (void)
{
  guint i = 0;

  G_LOCK (global_task_list);

  for (const GList *iter = global_task_list.head; iter; iter = iter->next)
    {
      IdeTask *self = iter->data;

      g_printerr ("[%02d]: %s %s\n", i++, self->name,
                  self->completed ? "completed" : "");
    }

  G_UNLOCK (global_task_list);
}
