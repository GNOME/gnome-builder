/* test-task.c
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

#include <libide-threading.h>

static gboolean
complete_int (gpointer data)
{
  IdeTask *task = data;
  ide_task_return_int (task, -123);
  return G_SOURCE_REMOVE;
}

static void
check_int (GObject      *object,
           GAsyncResult *result,
           gpointer      user_data)
{
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;
  gssize ret;

  g_assert (!object || G_IS_OBJECT (object));
  g_assert (IDE_IS_TASK (result));
  g_assert (main_loop != NULL);

  ret = ide_task_propagate_int (IDE_TASK (result), &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, -123);

  /* shoudln't switch to true until callback has exited */
  g_assert_false (ide_task_get_completed (IDE_TASK (result)));

  g_main_loop_quit (main_loop);
}

static void
test_ide_task_chain (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  IdeTask *task = ide_task_new (NULL, NULL, NULL, NULL);
  IdeTask *task2 = ide_task_new (NULL, NULL, check_int, g_main_loop_ref (main_loop));

  /* tests that we can chain the result from the first task to the
   * second task and get the same answer.
   */

  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);
  g_object_add_weak_pointer (G_OBJECT (task2), (gpointer *)&task2);

  ide_task_chain (task, task2);

  g_timeout_add (0, complete_int, task);
  g_main_loop_run (main_loop);

  g_assert_true (ide_task_get_completed (task));
  g_assert_true (ide_task_get_completed (task2));

  g_object_unref (task);
  g_object_unref (task2);

  g_assert_null (task);
  g_assert_null (task2);
}

static void
test_ide_task_basic (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  IdeTask *task = ide_task_new (NULL, NULL, check_int, g_main_loop_ref (main_loop));

  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_task_set_source_tag (task, test_ide_task_basic);
  g_assert (ide_task_get_source_tag (task) == test_ide_task_basic);

  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);

  g_timeout_add (0, complete_int, task);
  g_main_loop_run (main_loop);

  g_assert_true (ide_task_get_completed (task));
  g_object_unref (task);

  g_assert_null (task);
}

static void
test_ide_task_no_release (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  IdeTask *task = ide_task_new (NULL, NULL, check_int, g_main_loop_ref (main_loop));

  ide_task_set_release_on_propagate (task, FALSE);

  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&task);

  g_timeout_add (0, complete_int, task);
  g_main_loop_run (main_loop);

  g_assert_true (ide_task_get_completed (task));
  g_object_unref (task);

  g_assert_null (task);
}

static void
test_ide_task_serial (void)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  gboolean r;

  /*
   * This tests creating a task, returning, and propagating a value
   * serially without returning to the main loop. (the task will advance
   * the main context to make this work.
   */

  task = ide_task_new (NULL, NULL, NULL, NULL);
  g_assert_false (ide_task_get_completed (task));
  ide_task_return_boolean (task, TRUE);
  g_assert_false (ide_task_get_completed (task));
  r = ide_task_propagate_boolean (task, &error);
  g_assert_true (ide_task_get_completed (task));
  g_assert_no_error (error);
  g_assert_true (r);
}

static void
test_ide_task_delayed_chain (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(IdeTask) task2 = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GError) error = NULL;

  /* finish task 1, but it won't release resources since still need them
   * for future chaining.
   */
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_return_object (task, g_steal_pointer (&obj));
  g_assert_null (obj);
  obj = ide_task_propagate_object (task, &error);
  g_assert_nonnull (obj);

  /* try to chain a task, it should succeed since task still has the obj */
  ide_task_chain (task, task2);
  obj2 = ide_task_propagate_object (task2, &error);
  g_assert_no_error (error);
  g_assert_nonnull (obj2);
}

static void
test_ide_task_delayed_chain_fail (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(IdeTask) task2 = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GError) error = NULL;

  /* complete a task with an object, with release_on_propagate set */
  ide_task_return_object (task, g_steal_pointer (&obj));
  g_assert_null (obj);
  obj = ide_task_propagate_object (task, &error);
  g_assert_nonnull (obj);

  /* try to chain a task, it should fail since task released the obj */
  ide_task_chain (task, task2);
  obj2 = ide_task_propagate_object (task2, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_null (obj2);
}

static void
test_ide_task_null_object (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(IdeTask) task2 = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GObject) obj = NULL;
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(GError) error = NULL;

  /* Create a task, return a NULL object for a result. Ensure we got
   * NULL when propagating and no error.
   */
  ide_task_set_release_on_propagate (task, FALSE);
  ide_task_return_object (task, NULL);
  obj = ide_task_propagate_object (task, &error);
  g_assert_null (obj);
  g_assert_no_error (error);

  /* Now try to chain it, and make sure it is the same */
  ide_task_chain (task, task2);
  obj2 = ide_task_propagate_object (task2, &error);
  g_assert_no_error (error);
  g_assert_null (obj2);
}

typedef gchar FooStr;
GType foo_str_get_type (void);
G_DEFINE_BOXED_TYPE (FooStr, foo_str, g_strdup, g_free)

static void
test_ide_task_boxed (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;
  g_autofree gchar *ret = NULL;

  ide_task_return_boxed (task, foo_str_get_type (), g_strdup ("Hi there"));
  ret = ide_task_propagate_boxed (task, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (ret, ==, "Hi there");
}

static void
test_ide_task_get_cancellable (void)
{
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (NULL, cancellable, NULL, NULL);
  g_autoptr(GError) error = NULL;

  g_assert (cancellable == ide_task_get_cancellable (task));
  ide_task_return_int (task, 123);
  g_assert (cancellable == ide_task_get_cancellable (task));
  ide_task_propagate_int (task, &error);
  g_assert_no_error (error);
  g_assert (cancellable == ide_task_get_cancellable (task));
}

static void
test_ide_task_is_valid (void)
{
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(IdeTask) task2 = ide_task_new (obj, NULL, NULL, NULL);

  g_assert (ide_task_is_valid (task, NULL));
  g_assert (!ide_task_is_valid (task, obj));
  g_assert (!ide_task_is_valid (task2, NULL));
  g_assert (ide_task_is_valid (task2, obj));

  ide_task_return_int (task, 123);
  ide_task_return_int (task2, 123);
}

static void
test_ide_task_source_object (void)
{
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GObject) obj2 = NULL;
  g_autoptr(IdeTask) task = ide_task_new (obj, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;

  obj2 = g_async_result_get_source_object (G_ASYNC_RESULT (task));
  g_assert (obj == obj2);

  ide_task_return_boolean (task, TRUE);
  g_assert_true (ide_task_propagate_boolean (task, &error));
  g_assert_no_error (error);

  /* default release-on-propagate, source object released now */
  g_assert_null (g_async_result_get_source_object (G_ASYNC_RESULT (task)));
}

static void
test_ide_task_error (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_CONNECTED,
                             "Not connected");
  g_assert_false (ide_task_propagate_boolean (task, &error));
  g_assert_error (error,
                  G_IO_ERROR,
                  G_IO_ERROR_NOT_CONNECTED);
}

static void
typical_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;
  gboolean r;

  g_assert (object == NULL);
  g_assert (IDE_IS_TASK (result));
  g_assert (main_loop);

  r = ide_task_propagate_boolean (IDE_TASK (result), &error);
  g_assert_true (r);

  g_main_loop_quit (main_loop);
}

static gboolean
complete_in_main (gpointer data)
{
  g_autoptr(IdeTask) task = data;
  g_assert (IDE_IS_TASK (task));
  ide_task_return_boolean (task, TRUE);
  return G_SOURCE_REMOVE;
}

static void
test_ide_task_typical (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(IdeTask) task = NULL;
  IdeTask *finalize_check = NULL;

  task = ide_task_new (NULL, NULL, typical_cb, g_main_loop_ref (main_loop));

  /* life-cycle tracking */
  finalize_check = task;
  g_object_add_weak_pointer (G_OBJECT (task), (gpointer *)&finalize_check);

  /* simulate some async call */
  g_timeout_add (0, complete_in_main, g_steal_pointer (&task));

  g_main_loop_run (main_loop);

  g_assert_null (finalize_check);
}

static void
test_ide_task_thread_cb (IdeTask      *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  g_assert_nonnull (task);
  g_assert (IDE_IS_TASK (task));
  g_assert_nonnull (source_object);
  g_assert (G_IS_OBJECT (source_object));
  g_assert (G_IS_CANCELLABLE (cancellable));

  ide_task_return_int (task, -123);
}

static void
test_ide_task_thread (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (obj, cancellable, check_int, g_main_loop_ref (main_loop));

  ide_task_run_in_thread (task, test_ide_task_thread_cb);
  g_main_loop_run (main_loop);
}

static void
test_ide_task_thread_chained (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (obj, cancellable, check_int, g_main_loop_ref (main_loop));
  g_autoptr(IdeTask) task2 = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;
  gssize ret;

  ide_task_chain (task, task2);
  ide_task_run_in_thread (task, test_ide_task_thread_cb);
  g_main_loop_run (main_loop);

  ret = ide_task_propagate_int (task2, &error);
  g_assert_no_error (error);
  g_assert_cmpint (ret, ==, -123);
}

static void
inc_completed (IdeTask    *task,
               GParamSpec *pspec,
               gpointer    user_data)
{
  g_autoptr(GMainContext) context = NULL;
  guint *count = user_data;

  g_assert (IDE_IS_TASK (task));
  g_assert_nonnull (pspec);
  g_assert_cmpstr (pspec->name, ==, "completed");
  g_assert_nonnull (count);

  context = g_main_context_ref_thread_default ();
  g_assert (g_main_context_default () == context);

  (*count)++;
}

static void
test_ide_task_completed (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;
  guint count = 0;

  g_signal_connect (task, "notify::completed", G_CALLBACK (inc_completed), &count);
  ide_task_return_boolean (task, TRUE);
  g_assert_cmpint (count, ==, 0);
  g_assert_true (ide_task_propagate_boolean (task, &error));
  g_assert_no_error (error);
  g_assert_cmpint (count, ==, 1);
}

static void
test_ide_task_completed_threaded (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (obj, cancellable, check_int, g_main_loop_ref (main_loop));
  guint count = 0;

  g_signal_connect (task, "notify::completed", G_CALLBACK (inc_completed), &count);
  ide_task_run_in_thread (task, test_ide_task_thread_cb);
  g_main_loop_run (main_loop);
  g_assert_cmpint (count, ==, 1);
}

static void
test_ide_task_task_data (void)
{
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;
  gint *n = g_new0 (gint, 1);

  ide_task_set_task_data (task, n, g_free);
  g_assert (ide_task_get_task_data (task) == (gpointer)n);
  ide_task_return_boolean (task, TRUE);
  g_assert_nonnull (ide_task_get_task_data (task));
  g_assert_true (ide_task_propagate_boolean (task, &error));
  g_assert_no_error (error);
  /* after propagation, task data should be freed */
  g_assert_null (ide_task_get_task_data (task));
}

static void
test_ide_task_task_data_threaded (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (obj, cancellable, check_int, g_main_loop_ref (main_loop));
  gint *n = g_new0 (gint, 1);

  ide_task_set_task_data (task, n, g_free);
  ide_task_run_in_thread (task, test_ide_task_thread_cb);
  g_main_loop_run (main_loop);
  g_assert_null (ide_task_get_task_data (task));
}

static void
set_in_thread_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;
  IdeTask *task = (IdeTask *)result;

  g_assert (object == NULL);
  g_assert (IDE_IS_TASK (task));
  g_assert (main_loop != NULL);

  g_assert (ide_task_get_task_data (task) == GINT_TO_POINTER (0x1234));
  g_assert_true (ide_task_propagate_boolean (task, &error));
  g_assert_no_error (error);

  /* we should have this cleared until we return from this func */
  g_assert_nonnull (ide_task_get_task_data (task));
  g_assert (ide_task_get_task_data (task) == GINT_TO_POINTER (0x1234));

  g_main_loop_quit (main_loop);
}

static void
set_in_thread_worker (IdeTask      *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  g_assert (IDE_IS_TASK (task));
  g_assert (source_object == NULL);
  g_assert (task_data == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* its invalid to call set_task_data() after return, but okay here.
   * this obviously invalidates @task_data.
   */
  ide_task_set_task_data (task, GINT_TO_POINTER (0x1234), (GDestroyNotify)NULL);
  ide_task_return_boolean (task, TRUE);
}

static void
test_ide_task_task_data_set_in_thread (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(IdeTask) task = ide_task_new (NULL, NULL, set_in_thread_cb, g_main_loop_ref (main_loop));

  ide_task_run_in_thread (task, set_in_thread_worker);
  g_main_loop_run (main_loop);

  /* and now it should be cleared */
  g_assert_null (ide_task_get_task_data (task));
}

static void
test_ide_task_get_source_object (void)
{
  g_autoptr(GObject) obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_autoptr(IdeTask) task = ide_task_new (obj, NULL, NULL, NULL);
  g_autoptr(GError) error = NULL;

  g_assert_nonnull (ide_task_get_source_object (task));
  g_assert (obj == ide_task_get_source_object (task));

  ide_task_return_boolean (task, TRUE);

  g_assert_nonnull (ide_task_get_source_object (task));
  g_assert (obj == ide_task_get_source_object (task));

  g_assert_true (ide_task_propagate_boolean (task, &error));
  g_assert_null (ide_task_get_source_object (task));
}

static void
test_ide_task_check_cancellable (void)
{
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (NULL, cancellable, NULL, NULL);
  g_autoptr(IdeTask) task2 = ide_task_new (NULL, cancellable, NULL, NULL);
  g_autoptr(GError) error = NULL;

  ide_task_set_check_cancellable (task2, FALSE);

  g_cancellable_cancel (cancellable);
  ide_task_return_boolean (task, TRUE);
  ide_task_return_boolean (task2, TRUE);
  g_assert_false (ide_task_propagate_boolean (task, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);
  g_assert_true (ide_task_propagate_boolean (task2, &error));
  g_assert_no_error (error);
}

G_LOCK_DEFINE (cancel_lock);

static void
test_ide_task_return_on_cancel_worker (IdeTask      *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  g_assert (IDE_IS_TASK (task));
  g_assert (source_object == NULL);
  g_assert (G_IS_CANCELLABLE (cancellable));

  G_LOCK (cancel_lock);
  ide_task_return_boolean (task, TRUE);
  G_UNLOCK (cancel_lock);
}

static gboolean
idle_main_loop_quit (gpointer data)
{
  GMainLoop *main_loop = data;
  g_main_loop_quit (main_loop);
  return G_SOURCE_REMOVE;
}

static void
test_ide_task_return_on_cancel_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (object == NULL);
  g_assert (IDE_IS_TASK (result));

  g_assert_false (ide_task_propagate_boolean (IDE_TASK (result), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  G_UNLOCK (cancel_lock);

  /* sleep a bit to give the task a chance to hit error paths */
  g_timeout_add_full (50,
                      G_PRIORITY_DEFAULT,
                      idle_main_loop_quit,
                      g_steal_pointer (&main_loop),
                      (GDestroyNotify)g_main_loop_unref);
}

static void
test_ide_task_return_on_cancel (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(IdeTask) task = ide_task_new (NULL, cancellable,
                                          test_ide_task_return_on_cancel_cb,
                                          g_main_loop_ref (main_loop));

  G_LOCK (cancel_lock);

  ide_task_set_return_on_cancel (task, TRUE);
  ide_task_run_in_thread (task, test_ide_task_return_on_cancel_worker);

  g_cancellable_cancel (cancellable);
  g_main_loop_run (main_loop);
}

static void
test_ide_task_report_new_error_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(GMainLoop) main_loop = user_data;
  g_autoptr(GError) error = NULL;

  g_assert_null (object);
  g_assert (IDE_IS_TASK (result));

  g_assert_false (ide_task_propagate_boolean (IDE_TASK (result), &error));
  g_assert_error (error, G_IO_ERROR, 1234);

  g_main_loop_quit (main_loop);
}

static void
test_ide_task_report_new_error (void)
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);

  ide_task_report_new_error (NULL,
                             test_ide_task_report_new_error_cb,
                             g_main_loop_ref (main_loop),
                             test_ide_task_report_new_error,
                             G_IO_ERROR,
                             1234,
                             "Failure message");
  g_main_loop_run (main_loop);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Ide/Task/typical", test_ide_task_typical);
  g_test_add_func ("/Ide/Task/basic", test_ide_task_basic);
  g_test_add_func ("/Ide/Task/get-cancellable", test_ide_task_get_cancellable);
  g_test_add_func ("/Ide/Task/is-valid", test_ide_task_is_valid);
  g_test_add_func ("/Ide/Task/source-object", test_ide_task_source_object);
  g_test_add_func ("/Ide/Task/chain", test_ide_task_chain);
  g_test_add_func ("/Ide/Task/delayed-chain", test_ide_task_delayed_chain);
  g_test_add_func ("/Ide/Task/delayed-chain-fail", test_ide_task_delayed_chain_fail);
  g_test_add_func ("/Ide/Task/no-release", test_ide_task_no_release);
  g_test_add_func ("/Ide/Task/serial", test_ide_task_serial);
  g_test_add_func ("/Ide/Task/null-object", test_ide_task_null_object);
  g_test_add_func ("/Ide/Task/boxed", test_ide_task_boxed);
  g_test_add_func ("/Ide/Task/error", test_ide_task_error);
  g_test_add_func ("/Ide/Task/thread", test_ide_task_thread);
  g_test_add_func ("/Ide/Task/thread-chained", test_ide_task_thread_chained);
  g_test_add_func ("/Ide/Task/completed", test_ide_task_completed);
  g_test_add_func ("/Ide/Task/completed-threaded", test_ide_task_completed_threaded);
  g_test_add_func ("/Ide/Task/task-data", test_ide_task_task_data);
  g_test_add_func ("/Ide/Task/task-data-threaded", test_ide_task_task_data_threaded);
  g_test_add_func ("/Ide/Task/task-data-set-in-thread", test_ide_task_task_data_set_in_thread);
  g_test_add_func ("/Ide/Task/get-source-object", test_ide_task_get_source_object);
  g_test_add_func ("/Ide/Task/check-cancellable", test_ide_task_check_cancellable);
  g_test_add_func ("/Ide/Task/return-on-cancel", test_ide_task_return_on_cancel);
  g_test_add_func ("/Ide/Task/report-new-error", test_ide_task_report_new_error);

  return g_test_run ();
}
