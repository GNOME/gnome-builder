/* ide-test-manager.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <libide-io.h>
#include <libide-threading.h>
#include <libpeas/peas.h>

#include "ide-build-manager.h"
#include "ide-pipeline.h"
#include "ide-foundry-compat.h"
#include "ide-test-manager.h"
#include "ide-test-private.h"
#include "ide-test-provider.h"

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
 *
 * Since: 3.32
 */

struct _IdeTestManager
{
  IdeObject         parent_instance;

  PeasExtensionSet *providers;
  GPtrArray        *tests_by_provider;
  GtkTreeStore     *tests_store;
  GCancellable     *cancellable;
  VtePty           *pty;
  gint              child_pty;
  gint              n_active;
};

typedef struct
{
  IdeTestProvider *provider;
  GPtrArray       *tests;
} TestsByProvider;

typedef struct
{
  GQueue queue;
  guint  n_active;
} RunAllTaskData;

enum {
  PROP_0,
  PROP_LOADING,
  N_PROPS
};

static void initable_iface_init              (GInitableIface *iface);
static void ide_test_manager_actions_run_all (IdeTestManager *self,
                                              GVariant       *param);
static void ide_test_manager_actions_reload  (IdeTestManager *self,
                                              GVariant       *param);
static void ide_test_manager_actions_cancel  (IdeTestManager *self,
                                              GVariant       *param);

DZL_DEFINE_ACTION_GROUP (IdeTestManager, ide_test_manager, {
  { "cancel", ide_test_manager_actions_cancel },
  { "run-all", ide_test_manager_actions_run_all },
  { "reload-tests", ide_test_manager_actions_reload },
})

G_DEFINE_TYPE_WITH_CODE (IdeTestManager, ide_test_manager, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                                ide_test_manager_init_action_group))

static GParamSpec *properties [N_PROPS];

static void
tests_by_provider_free (gpointer data)
{
  TestsByProvider *info = data;

  g_clear_pointer (&info->tests, g_ptr_array_unref);
  g_clear_object (&info->provider);
  g_slice_free (TestsByProvider, info);
}

static void
ide_test_manager_destroy (IdeObject *object)
{
  IdeTestManager *self = (IdeTestManager *)object;

  if (self->child_pty != -1)
    {
      close (self->child_pty);
      self->child_pty = -1;
    }

  if (self->tests_store != NULL)
    {
      gtk_tree_store_clear (self->tests_store);
      g_clear_object (&self->tests_store);
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->providers);
  g_clear_pointer (&self->tests_by_provider, g_ptr_array_unref);

  g_clear_object (&self->pty);

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
    case PROP_LOADING:
      g_value_set_boolean (value, ide_test_manager_get_loading (self));
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
   * IdeTestManager:loading:
   *
   * The "loading" property denotes if a test provider is busy loading
   * tests in the background.
   *
   * Since: 3.32
   */
  properties [PROP_LOADING] =
    g_param_spec_boolean ("loading",
                          "Loading",
                          "If a test provider is loading tests",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_test_manager_init (IdeTestManager *self)
{
  self->child_pty = -1;
  self->cancellable = g_cancellable_new ();
  self->tests_by_provider = g_ptr_array_new_with_free_func (tests_by_provider_free);
  self->tests_store = gtk_tree_store_new (2, G_TYPE_STRING, IDE_TYPE_TEST);

  ide_test_manager_set_action_enabled (self, "cancel", FALSE);
}

static void
ide_test_manager_locate_group (IdeTestManager *self,
                               GtkTreeIter    *iter,
                               const gchar    *group)
{
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (iter != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->tests_store), iter))
    {
      do
        {
          g_autofree gchar *row_group = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->tests_store), iter,
                              IDE_TEST_COLUMN_GROUP, &row_group,
                              -1);

          if (ide_str_equal0 (row_group, group))
            return;
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->tests_store), iter));
    }

  /* TODO: Sort groups by name? */

  gtk_tree_store_append (self->tests_store, iter, NULL);
  gtk_tree_store_set (self->tests_store, iter,
                      IDE_TEST_COLUMN_GROUP, group,
                      -1);
}

static void
ide_test_manager_test_notify_status (IdeTestManager *self,
                                     GParamSpec     *pspec,
                                     IdeTest        *test)
{
  const gchar *group;
  GtkTreeIter parent;
  GtkTreeIter iter;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (IDE_IS_TEST (test));

  group = ide_test_get_group (test);

  ide_test_manager_locate_group (self, &parent, group);

  if (gtk_tree_model_iter_children (GTK_TREE_MODEL (self->tests_store), &iter, &parent))
    {
      do
        {
          g_autoptr(IdeTest) row_test = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->tests_store), &iter,
                              IDE_TEST_COLUMN_TEST, &row_test,
                              -1);

          if (row_test == test)
            {
              GtkTreePath *path;

              path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->tests_store), &iter);
              gtk_tree_model_row_changed (GTK_TREE_MODEL (self->tests_store), path, &iter);
              gtk_tree_path_free (path);

              break;
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->tests_store), &iter));
    }
}

static void
ide_test_manager_add_test (IdeTestManager        *self,
                           const TestsByProvider *info,
                           guint                  position,
                           IdeTest               *test)
{
  const gchar *group;
  GtkTreeIter iter;
  GtkTreeIter parent;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (info != NULL);
  g_assert (IDE_IS_TEST (test));

  g_ptr_array_insert (info->tests, position, g_object_ref (test));

  group = ide_test_get_group (test);

  ide_test_manager_locate_group (self, &parent, group);
  gtk_tree_store_append (self->tests_store, &iter, &parent);
  gtk_tree_store_set (self->tests_store, &iter,
                      IDE_TEST_COLUMN_GROUP, NULL,
                      IDE_TEST_COLUMN_TEST, test,
                      -1);

  g_signal_connect_object (test,
                           "notify::status",
                           G_CALLBACK (ide_test_manager_test_notify_status),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
ide_test_manager_remove_test (IdeTestManager        *self,
                              const TestsByProvider *info,
                              IdeTest               *test)
{
  const gchar *group;
  GtkTreeIter iter;
  GtkTreeIter parent;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (info != NULL);
  g_assert (IDE_IS_TEST (test));

  group = ide_test_get_group (test);

  ide_test_manager_locate_group (self, &parent, group);

  if (gtk_tree_model_iter_children (GTK_TREE_MODEL (self->tests_store), &iter, &parent))
    {
      do
        {
          g_autoptr(IdeTest) row = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->tests_store), &iter,
                              IDE_TEST_COLUMN_TEST, &row,
                              -1);

          if (row == test)
            {
              g_signal_handlers_disconnect_by_func (test,
                                                    G_CALLBACK (ide_test_manager_test_notify_status),
                                                    self);
              gtk_tree_store_remove (self->tests_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->tests_store), &iter));
    }

  g_ptr_array_remove (info->tests, test);

  IDE_EXIT;
}

static void
ide_test_manager_provider_items_changed (IdeTestManager  *self,
                                         guint            position,
                                         guint            removed,
                                         guint            added,
                                         IdeTestProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (IDE_IS_TEST_PROVIDER (provider));

  for (guint i = 0; i < self->tests_by_provider->len; i++)
    {
      const TestsByProvider *info = g_ptr_array_index (self->tests_by_provider, i);

      if (info->provider == provider)
        {
          /* Remove tests from cache that were deleted */
          for (guint j = 0; j < removed; j++)
            {
              IdeTest *test = g_ptr_array_index (info->tests, position);

              g_assert (IDE_IS_TEST (test));
              ide_test_manager_remove_test (self, info, test);
            }

          /* Add tests to cache that were added */
          for (guint j = 0; j < added; j++)
            {
              g_autoptr(IdeTest) test = NULL;

              test = g_list_model_get_item (G_LIST_MODEL (provider), position + j);
              g_assert (IDE_IS_TEST (test));
              ide_test_manager_add_test (self, info, position + j, test);
            }
        }
    }

  IDE_EXIT;
}

static void
ide_test_manager_provider_notify_loading (IdeTestManager  *self,
                                          GParamSpec      *pspec,
                                          IdeTestProvider *provider)
{
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (IDE_IS_TEST_PROVIDER (provider));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOADING]);
}

static void
ide_test_manager_provider_added (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 PeasExtension    *exten,
                                 gpointer          user_data)
{
  IdeTestManager *self = user_data;
  IdeTestProvider *provider = (IdeTestProvider *)exten;
  TestsByProvider *tests;
  guint len;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TEST_PROVIDER (provider));
  g_assert (G_IS_LIST_MODEL (provider));
  g_assert (IDE_IS_TEST_MANAGER (self));

  tests = g_slice_new0 (TestsByProvider);
  tests->provider = g_object_ref (provider);
  tests->tests = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (self->tests_by_provider, tests);

  g_signal_connect_swapped (provider,
                            "items-changed",
                            G_CALLBACK (ide_test_manager_provider_items_changed),
                            self);
  g_signal_connect_swapped (provider,
                            "notify::loading",
                            G_CALLBACK (ide_test_manager_provider_notify_loading),
                            self);

  len = g_list_model_get_n_items (G_LIST_MODEL (provider));
  ide_test_manager_provider_items_changed (self, 0, 0, len, provider);

  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
ide_test_manager_provider_removed (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   PeasExtension    *exten,
                                   gpointer          user_data)
{
  IdeTestManager *self = user_data;
  IdeTestProvider *provider = (IdeTestProvider *)exten;

  IDE_ENTRY;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TEST_PROVIDER (provider));
  g_assert (IDE_IS_TEST_MANAGER (self));

  for (guint i = 0; i < self->tests_by_provider->len; i++)
    {
      const TestsByProvider *info = g_ptr_array_index (self->tests_by_provider, i);

      if (info->provider == provider)
        {
          g_ptr_array_remove_index (self->tests_by_provider, i);
          break;
        }
    }

  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_test_manager_provider_items_changed),
                                        self);
  g_signal_handlers_disconnect_by_func (provider,
                                        G_CALLBACK (ide_test_manager_provider_notify_loading),
                                        self);

  ide_object_destroy (IDE_OBJECT (provider));

  IDE_EXIT;
}

static gboolean
ide_test_manager_initiable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  IdeTestManager *self = (IdeTestManager *)initable;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_TEST_PROVIDER,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (ide_test_manager_provider_added),
                    self);

  g_signal_connect (self->providers,
                    "extension-removed",
                    G_CALLBACK (ide_test_manager_provider_removed),
                    self);

  peas_extension_set_foreach (self->providers,
                              ide_test_manager_provider_added,
                              self);

  IDE_RETURN (TRUE);
}

static void
initable_iface_init (GInitableIface *iface)
{
  iface->init = ide_test_manager_initiable_init;
}

static void
ide_test_manager_run_all_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeTestManager *self = (IdeTestManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTest) test = NULL;
  RunAllTaskData *task_data;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  cancellable = g_task_get_cancellable (task);
  task_data = g_task_get_task_data (task);
  g_assert (task_data != NULL);
  g_assert (task_data->n_active > 0);

  if (!ide_test_manager_run_finish (self, result, &error))
    g_message ("%s", error->message);

  test = g_queue_pop_head (&task_data->queue);

  if (test != NULL)
    {
      task_data->n_active++;
      ide_test_manager_run_async (self,
                                  test,
                                  cancellable,
                                  ide_test_manager_run_all_cb,
                                  g_object_ref (task));
    }

  task_data->n_active--;

  if (task_data->n_active == 0)
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
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
 *
 * Since: 3.32
 */
void
ide_test_manager_run_all_async (IdeTestManager      *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  RunAllTaskData *task_data;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_test_manager_run_all_async);

  task_data = g_new0 (RunAllTaskData, 1);
  g_task_set_task_data (task, task_data, g_free);

  for (guint i = 0; i < self->tests_by_provider->len; i++)
    {
      TestsByProvider *info = g_ptr_array_index (self->tests_by_provider, i);

      for (guint j = 0; j < info->tests->len; j++)
        {
          IdeTest *test = g_ptr_array_index (info->tests, j);

          g_queue_push_tail (&task_data->queue, g_object_ref (test));
        }
    }

  task_data->n_active = MIN (MAX_UNIT_TESTS, task_data->queue.length);

  if (task_data->n_active == 0)
    {
      g_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  for (guint i = 0; i < MAX_UNIT_TESTS; i++)
    {
      g_autoptr(IdeTest) test = g_queue_pop_head (&task_data->queue);

      if (test == NULL)
        break;

      ide_test_manager_run_async (self,
                                  test,
                                  cancellable,
                                  ide_test_manager_run_all_cb,
                                  g_object_ref (task));
    }

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
 *
 * Since: 3.32
 */
gboolean
ide_test_manager_run_all_finish (IdeTestManager  *self,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_task_completed_cb (IdeTestManager *self,
                       GParamSpec     *pspec,
                       IdeTask        *task)
{
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (G_IS_TASK (task));
  g_assert (self->n_active > 0);

  self->n_active--;

  ide_test_manager_set_action_enabled (self, "cancel", self->n_active > 0);
}

static void
ide_test_manager_run_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeTestProvider *provider = (IdeTestProvider *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_test_provider_run_finish (provider, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
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
 *
 * Since: 3.32
 */
void
ide_test_manager_run_async (IdeTestManager      *self,
                            IdeTest             *test,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdePipeline *pipeline;
  IdeTestProvider *provider;
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (IDE_IS_TEST (test));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_test_manager_run_async);

  self->n_active++;
  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (run_task_completed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  ide_test_manager_set_action_enabled (self, "cancel", TRUE);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Pipeline is not ready, cannot run test");
      IDE_EXIT;
    }

  provider = _ide_test_get_provider (test);

  if (self->pty == NULL)
    {
      g_autoptr(GError) error = NULL;

      if (!(self->pty = vte_pty_new_sync (VTE_PTY_DEFAULT, cancellable, &error)))
        {
          g_task_return_error (task, g_steal_pointer (&error));
          IDE_EXIT;
        }
    }

  ide_test_provider_run_async (provider,
                               test,
                               pipeline,
                               self->pty,
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
 *
 * Since: 3.32
 */
gboolean
ide_test_manager_run_finish (IdeTestManager  *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_test_manager_actions_run_all (IdeTestManager *self,
                                  GVariant       *param)
{
  g_assert (IDE_IS_TEST_MANAGER (self));

  ide_test_manager_run_all_async (self, NULL, NULL, NULL);
}

static void
ide_test_manager_actions_reload (IdeTestManager *self,
                                 GVariant       *param)
{
  g_assert (IDE_IS_TEST_MANAGER (self));

  gtk_tree_store_clear (self->tests_store);

  for (guint i = 0; i < self->tests_by_provider->len; i++)
    {
      const TestsByProvider *info = g_ptr_array_index (self->tests_by_provider, i);

      ide_test_provider_reload (info->provider);
    }
}

GtkTreeModel *
_ide_test_manager_get_model (IdeTestManager *self)
{
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  return GTK_TREE_MODEL (self->tests_store);
}

static void
ide_test_manager_get_loading_cb (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 PeasExtension    *exten,
                                 gpointer          user_data)
{
  IdeTestProvider *provider = (IdeTestProvider *)exten;
  gboolean *loading = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TEST_PROVIDER (provider));
  g_assert (loading != NULL);

  *loading |= ide_test_provider_get_loading (provider);
}

gboolean
ide_test_manager_get_loading (IdeTestManager *self)
{
  gboolean loading = FALSE;

  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);

  peas_extension_set_foreach (self->providers,
                              ide_test_manager_get_loading_cb,
                              &loading);

  return loading;
}

/**
 * ide_test_manager_get_tests:
 * @self: a #IdeTestManager
 * @path: (nullable): the path to the test or %NULL for the root path
 *
 * Locates and returns any #IdeTest that is found as a direct child
 * of @path.
 *
 * Returns: (transfer full) (element-type IdeTest): an array of #IdeTest
 *
 * Since: 3.32
 */
GPtrArray *
ide_test_manager_get_tests (IdeTestManager *self,
                            const gchar    *path)
{
  GPtrArray *ret;
  GtkTreeIter iter;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  ret = g_ptr_array_new ();

  if (path == NULL)
    {
      if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->tests_store), &iter))
        goto failure;
    }
  else
    {
      GtkTreeIter parent;

      ide_test_manager_locate_group (self, &parent, path);

      if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (self->tests_store), &iter, &parent))
        goto failure;
    }

  do
    {
      IdeTest *test = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (self->tests_store), &iter,
                          IDE_TEST_COLUMN_TEST, &test,
                          -1);
      if (test != NULL)
        g_ptr_array_add (ret, g_steal_pointer (&test));
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->tests_store), &iter));

failure:
  return g_steal_pointer (&ret);
}

/**
 * ide_test_manager_get_folders:
 * @self: a #IdeTestManager
 * @path: (nullable): the path to the test or %NULL for the root path
 *
 * Gets the sub-paths of @path that are not individual tests.
 *
 * Returns: (transfer full) (array zero-terminated=1): an array of strings
 *   describing available sub-paths to @path.
 *
 * Since: 3.32
 */
gchar **
ide_test_manager_get_folders (IdeTestManager *self,
                              const gchar    *path)
{
  static const gchar *empty[] = { NULL };
  GPtrArray *ret;
  GtkTreeIter iter;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  ret = g_ptr_array_new ();

  if (path == NULL)
    {
      if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->tests_store), &iter))
        return g_strdupv ((gchar **)empty);
    }
  else
    {
      GtkTreeIter parent;

      ide_test_manager_locate_group (self, &parent, path);

      if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (self->tests_store), &iter, &parent))
        return g_strdupv ((gchar **)empty);
    }

  do
    {
      gchar *group = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (self->tests_store), &iter,
                          IDE_TEST_COLUMN_GROUP, &group,
                          -1);
      if (group != NULL)
        g_ptr_array_add (ret, g_steal_pointer (&group));
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->tests_store), &iter));

  g_ptr_array_add (ret, NULL);

  return (gchar **)g_ptr_array_free (ret, FALSE);
}

static void
ide_test_manager_ensure_loaded_cb (IdeTestManager *self,
                                   GParamSpec     *pspec,
                                   IdeTask        *task)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (IDE_IS_TASK (task));

  if (!ide_test_manager_get_loading (self))
    {
      g_signal_handlers_disconnect_by_func (self,
                                            G_CALLBACK (ide_test_manager_ensure_loaded_cb),
                                            task);
      ide_task_return_boolean (task, TRUE);
    }
}

/**
 * ide_test_manager_ensure_loaded_async:
 * @self: a #IdeTestManager
 *
 * Calls @callback after the test manager has loaded tests.
 *
 * If the test manager has already loaded tests, then @callback will
 * be called after returning to the main loop.
 *
 * Since: 3.32
 */
void
ide_test_manager_ensure_loaded_async (IdeTestManager      *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_test_manager_ensure_loaded_async);

  if (ide_test_manager_get_loading (self))
    {
      g_signal_connect_data (self,
                             "notify::loading",
                             G_CALLBACK (ide_test_manager_ensure_loaded_cb),
                             g_steal_pointer (&task),
                             (GClosureNotify)g_object_unref,
                             0);
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

gboolean
ide_test_manager_ensure_loaded_finish (IdeTestManager  *self,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_test_manager_get_pty:
 * @self: a #IdeTestManager
 *
 * Gets the #VtePty to use for running unit tests.
 *
 * Returns: (transfer none): a #VtePty
 *
 * Since: 3.32
 */
VtePty *
ide_test_manager_get_pty (IdeTestManager *self)
{
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  if (self->pty == NULL)
    self->pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);

  return self->pty;
}

/**
 * ide_test_manager_open_pty:
 * @self: a #IdeTestManager
 *
 * Gets a FD that maps to the child side of the PTY device.
 *
 * Returns: a new FD or -1 on failure
 *
 * Since: 3.34
 */
gint
ide_test_manager_open_pty (IdeTestManager *self)
{
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), -1);

  if (self->child_pty == -1)
    {
      VtePty *pty = ide_test_manager_get_pty (self);
      self->child_pty = ide_pty_intercept_create_slave (vte_pty_get_fd (pty), TRUE);
    }

  return dup (self->child_pty);
}

/**
 * ide_test_manager_get_cancellable:
 * @self: a #IdeTestManager
 *
 * Gets the cancellable for the test manager which will be cancelled
 * when the cancel action is called.
 *
 * Returns: (transfer none): a #GCancellable
 *
 * Since: 3.34
 */
GCancellable *
ide_test_manager_get_cancellable (IdeTestManager *self)
{
  g_return_val_if_fail (IDE_IS_TEST_MANAGER (self), NULL);

  return self->cancellable;
}

static void
ide_test_manager_actions_cancel (IdeTestManager *self,
                                 GVariant       *param)
{
  g_assert (IDE_IS_TEST_MANAGER (self));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
}
