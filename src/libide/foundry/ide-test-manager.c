/* ide-test-manager.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test-manager"

#include "config.h"

#include <libpeas.h>

#include <libide-core.h>
#include <libide-io.h>
#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-build-manager.h"
#include "ide-foundry-compat.h"
#include "ide-pipeline.h"
#include "ide-pty.h"
#include "ide-run-context.h"
#include "ide-run-command.h"
#include "ide-run-commands.h"
#include "ide-run-manager.h"
#include "ide-test-manager.h"
#include "ide-test-private.h"

#define MAX_UNIT_TESTS 4

/**
 * SECTION:ide-test-manager
 * @title: IdeTestManager
 * @short_description: Unit test discover and execution manager
 *
 * The #IdeTestManager is responsible for loading unit test provider
 * plugins (via the #IdeTestProvider interface) and running those unit
 * tests on behalf of the user.
 *
 * You can access the test manager using ide_context_get_text_manager()
 * using the #IdeContext for the loaded project.
 */

struct _IdeTestManager
{
  IdeObject           parent_instance;
  GtkFilterListModel *filtered;
  IdeCachedListModel *tests;
  VtePty             *pty;
  GCancellable       *cancellable;
  IdePtyIntercept     intercept;
  int                 pty_producer;
  guint               n_active;
};

typedef struct
{
  IdePipeline *pipeline;
  GPtrArray   *tests;
  VtePty      *pty;
  guint        n_active;
} RunAll;

static void ide_test_manager_actions_cancel   (IdeTestManager *self,
                                               GVariant       *param);
static void ide_test_manager_actions_test     (IdeTestManager *self,
                                               GVariant       *param);
static void ide_test_manager_actions_test_all (IdeTestManager *self,
                                               GVariant       *param);

IDE_DEFINE_ACTION_GROUP (IdeTestManager, ide_test_manager, {
  { "test", ide_test_manager_actions_test, "s" },
  { "test-all", ide_test_manager_actions_test_all },
  { "cancel", ide_test_manager_actions_cancel },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeTestManager, ide_test_manager, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_test_manager_init_action_group))

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

enum {
  BEGIN_TEST_ALL,
  END_TEST_ALL,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
filter_tests_func (gpointer item,
                   gpointer user_data)
{
  IdeRunCommand *run_command = item;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMAND (run_command));

  return ide_run_command_get_kind (run_command) == IDE_RUN_COMMAND_KIND_TEST;
}

static void
run_all_free (RunAll *state)
{
  g_assert (state != NULL);
  g_assert (state->n_active == 0);

  g_clear_pointer (&state->tests, g_ptr_array_unref);
  g_clear_object (&state->pipeline);
  g_clear_object (&state->pty);

  g_slice_free (RunAll, state);
}

static GCancellable *
get_cancellable (IdeTestManager *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));

  if (g_cancellable_is_cancelled (self->cancellable))
    g_clear_object (&self->cancellable);

  if (self->cancellable == NULL)
    self->cancellable = g_cancellable_new ();

  return self->cancellable;
}

static void
ide_test_manager_actions_cancel (IdeTestManager *self,
                                 GVariant       *param)
{
  g_cancellable_cancel (self->cancellable);
}

static void
ide_test_manager_actions_test (IdeTestManager *self,
                               GVariant       *param)
{
  GCancellable *cancellable;
  GListModel *tests;
  const char *test_id;
  guint n_items;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));

  cancellable = get_cancellable (self);

  test_id = g_variant_get_string (param, NULL);
  tests = ide_test_manager_list_tests (self);
  n_items = g_list_model_get_n_items (tests);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeTest) test = g_list_model_get_item (tests, i);

      if (ide_str_equal0 (test_id, ide_test_get_id (test)))
        {
          ide_test_manager_run_async (self, test, cancellable, NULL, NULL);
          IDE_EXIT;
        }
    }

  IDE_EXIT;
}

static void
ide_test_manager_actions_test_all (IdeTestManager *self,
                                   GVariant       *param)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (param == NULL);

  ide_test_manager_run_all_async (self, get_cancellable (self), NULL, NULL);

  IDE_EXIT;
}

static gpointer
map_run_command_to_test (gpointer item,
                         gpointer user_data)
{
  g_autoptr(IdeRunCommand) run_command = item;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_COMMAND (run_command));

  return ide_test_new (run_command);
}

static void
ide_test_manager_destroy (IdeObject *object)
{
  IdeTestManager *self = (IdeTestManager *)object;
  g_auto(IdePtyFd) fd = IDE_PTY_FD_INVALID;

  g_clear_object (&self->cancellable);
  g_clear_object (&self->filtered);
  g_clear_object (&self->tests);

  g_clear_object (&self->pty);
  fd = pty_fd_steal (&self->pty_producer);

  if (IDE_IS_PTY_INTERCEPT (&self->intercept))
    ide_pty_intercept_clear (&self->intercept);

  IDE_OBJECT_CLASS (ide_test_manager_parent_class)->destroy (object);
}

static void
ide_test_manager_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTestManager *self = IDE_TEST_MANAGER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, ide_test_manager_list_tests (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_manager_class_init (IdeTestManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_test_manager_get_property;

  i_object_class->destroy = ide_test_manager_destroy;

  /**
   * IdeTestManager:model:
   *
   * The "model" property contains a #GListModel of #IdeTest.
   *
   * Fetching this property will not cause the #GListModel to be
   * populated. That is only done by calling ide_test_manager_list_tests().
   *
   * This may be a more convenient way to get access to the model when you
   * do not want the tests to autopopulate just to be bound to UI elements.
   */
  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [BEGIN_TEST_ALL] =
    g_signal_new ("begin-test-all",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [BEGIN_TEST_ALL],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  signals [END_TEST_ALL] =
    g_signal_new ("end-test-all",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [END_TEST_ALL],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_test_manager_init (IdeTestManager *self)
{
  GtkCustomFilter *filter;
  GtkMapListModel *map;
  int consumer_fd;

  self->pty = ide_pty_new_sync (NULL);
  self->pty_producer = -1;

  /* Now create intercept, which we'll use to apply PTY
   * to all the spawned processes instead of our VtePty.
   */
  consumer_fd = vte_pty_get_fd (self->pty);
  ide_pty_intercept_init (&self->intercept, consumer_fd, NULL);

  filter = gtk_custom_filter_new (filter_tests_func, NULL, NULL);
  self->filtered = gtk_filter_list_model_new (NULL, GTK_FILTER (filter));
  map = gtk_map_list_model_new (g_object_ref (G_LIST_MODEL (self->filtered)),
                                map_run_command_to_test,
                                NULL, NULL);
  self->tests = ide_cached_list_model_new (G_LIST_MODEL (map));
}

static void
ide_test_manager_run_all_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeTestManager *self = (IdeTestManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  RunAll *state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);
  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->n_active > 0);
  g_assert (state->tests != NULL);

  if (!ide_test_manager_run_finish (self, result, &error))
    g_message ("%s", error->message);

  if (state->tests->len > 0 &&
      !g_cancellable_is_cancelled (cancellable))
    {
      g_autoptr(IdeTest) next_test = g_ptr_array_steal_index (state->tests, state->tests->len-1);

      state->n_active++;

      ide_test_manager_run_async (self,
                                  next_test,
                                  cancellable,
                                  ide_test_manager_run_all_cb,
                                  g_object_ref (task));
    }

  state->n_active--;

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_test_manager_emit_end_test_all (IdeTestManager *self)
{
  g_signal_emit (self, signals [END_TEST_ALL], 0);
}

/**
 * ide_test_manager_run_all_async:
 * @self: An #IdeTestManager
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Executes all tests in an undefined order.
 *
 * Upon completion, @callback will be executed which must call
 * ide_test_manager_run_all_finish() to get the result.
 *
 * Note that the individual test result information will be attached
 * to the specific #IdeTest instances.
 */
void
ide_test_manager_run_all_async (IdeTestManager      *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  GListModel *tests;
  IdeContext *context;
  RunAll *state;
  guint n_items;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_test_manager_run_all_async);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_INITIALIZED,
                                 "Cannot run test until pipeline is ready");
      IDE_EXIT;
    }

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_test_manager_emit_end_test_all),
                           self,
                           G_CONNECT_SWAPPED);

  tests = ide_test_manager_list_tests (self);
  n_items = g_list_model_get_n_items (tests);

  ar = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = n_items; i > 0; i--)
    g_ptr_array_add (ar, g_list_model_get_item (tests, i-1));

  state = g_slice_new0 (RunAll);
  state->tests = g_ptr_array_ref (ar);
  state->pipeline = g_object_ref (pipeline);
  state->n_active = 0;
  ide_task_set_task_data (task, state, run_all_free);

  for (guint i = 0; i < MAX_UNIT_TESTS && ar->len > 0; i++)
    {
      g_autoptr(IdeTest) test = g_ptr_array_steal_index (state->tests, ar->len-1);

      state->n_active++;

      ide_test_manager_run_async (self,
                                  test,
                                  cancellable,
                                  ide_test_manager_run_all_cb,
                                  g_object_ref (task));
    }

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);

  g_signal_emit (self, signals[BEGIN_TEST_ALL], 0);

  IDE_EXIT;
}

/**
 * ide_test_manager_run_all_finish:
 * @self: An #IdeTestManager
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to execute all unit tests.
 *
 * A return value of %TRUE does not indicate that all tests succeeded,
 * only that all tests were executed. Individual test failures will be
 * attached to the #IdeTest instances.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_test_manager_run_all_finish (IdeTestManager  *self,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_test_manager_run_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeTest *test = (IdeTest *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST (test));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_test_run_finish (test, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_test_manager_test_notify_completed_cb (IdeTestManager *self,
                                           GParamSpec     *pspec,
                                           IdeTask        *task)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (IDE_IS_TASK (task));

  self->n_active--;

  if (self->n_active == 0)
    ide_test_manager_set_action_enabled (self, "cancel", FALSE);
}

/**
 * ide_test_manager_run_async:
 * @self: An #IdeTestManager
 * @test: An #IdeTest
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Executes a single unit test, asynchronously.
 *
 * The caller can access the result of the operation from @callback
 * by calling ide_test_manager_run_finish() with the provided result.
 */
void
ide_test_manager_run_async (IdeTestManager      *self,
                            IdeTest             *test,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (IDE_IS_TEST (test));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_test_manager_run_async);

  self->n_active++;

  if (self->n_active == 1)
    ide_test_manager_set_action_enabled (self, "cancel", TRUE);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_test_manager_test_notify_completed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (self->pty_producer == -1)
    {
      IdePtyFd consumer_fd = ide_pty_intercept_get_fd (&self->intercept);
      self->pty_producer = ide_pty_intercept_create_producer (consumer_fd, TRUE);
    }

  if (pipeline == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_INITIALIZED,
                               "Pipeline is not ready, cannot run test");
  else
    ide_test_run_async (test,
                        pipeline,
                        self->pty_producer,
                        cancellable,
                        ide_test_manager_run_cb,
                        g_steal_pointer (&task));

  IDE_EXIT;
}

/**
 * ide_test_manager_run_finish:
 * @self: An #IdeTestManager
 * @result: The #GAsyncResult provided to callback
 * @error: A location for a #GError, or %NULL
 *
 * Completes a request to ide_test_manager_run_finish().
 *
 * When this function returns %TRUE, it does not indicate that the test
 * succeeded; only that the test was executed. Thest #IdeTest instance
 * itself will contain information about the success of the test.
 *
 * Returns: %TRUE if the test was executed; otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_test_manager_run_finish (IdeTestManager  *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_test_manager_get_pty:
 * @self: a #IdeTestManager
 *
 * Gets the #VtePty to use for running unit tests.
 *
 * Returns: (transfer none): a #VtePty
 */
VtePty *
ide_test_manager_get_pty (IdeTestManager *self)
{
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  return self->pty;
}

/**
 * ide_test_manager_list_tests:
 * @self: a #IdeTestManager
 *
 * Gets a #GListModel of #IdeTest.
 *
 * This will return a #GListModel immediately, but that list may not complete
 * until some time in the future based on how quickly various
 * #IdeRunCommandProvider return commands.
 *
 * Returns: (transfer none): an #GListModel of #IdeTest
 */
GListModel *
ide_test_manager_list_tests (IdeTestManager *self)
{
  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  if (gtk_filter_list_model_get_model (self->filtered) == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeRunCommands *run_commands = ide_run_commands_from_context (context);

      gtk_filter_list_model_set_model (self->filtered, G_LIST_MODEL (run_commands));
    }

  IDE_RETURN (G_LIST_MODEL (self->tests));
}
