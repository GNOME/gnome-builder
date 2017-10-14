/* ide-test-manager.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test-manager"

#include <dazzle.h>
#include <libpeas/peas.h>

#include "ide-debug.h"

#include "testing/ide-test-manager.h"
#include "testing/ide-test-private.h"
#include "testing/ide-test-provider.h"

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
 * Since: 3.28
 */

struct _IdeTestManager
{
  IdeObject         parent_instance;

  PeasExtensionSet *providers;
  GPtrArray        *tests_by_provider;
  GtkTreeStore     *tests_store;
};

typedef struct
{
  IdeTestProvider *provider;
  GPtrArray       *tests;
} TestsByProvider;

enum {
  PROP_0,
  PROP_LOADING,
  N_PROPS
};

static void initable_iface_init              (GInitableIface *iface);
static void ide_test_manager_actions_run_all (IdeTestManager *self,
                                              GVariant       *param);

DZL_DEFINE_ACTION_GROUP (IdeTestManager, ide_test_manager, {
  { "run-all", ide_test_manager_actions_run_all },
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
ide_test_manager_dispose (GObject *object)
{
  IdeTestManager *self = (IdeTestManager *)object;

  g_clear_object (&self->providers);
  g_clear_object (&self->tests_store);
  g_clear_pointer (&self->tests_by_provider, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_test_manager_parent_class)->dispose (object);
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

  object_class->dispose = ide_test_manager_dispose;
  object_class->get_property = ide_test_manager_get_property;

  /**
   * IdeTestManager:loading:
   *
   * The "loading" property denotes if a test provider is busy loading
   * tests in the background.
   *
   * Since: 3.28
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
  self->tests_by_provider = g_ptr_array_new_with_free_func (tests_by_provider_free);
  self->tests_store = gtk_tree_store_new (2, G_TYPE_STRING, IDE_TYPE_TEST);
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
              ide_test_manager_remove_test (self, info, test);
            }

          /* Add tests to cache that were added */
          for (guint j = 0; j < added; j++)
            {
              g_autoptr(IdeTest) test = NULL;

              test = g_list_model_get_item (G_LIST_MODEL (provider), position + j);
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

  IDE_EXIT;
}

static gboolean
ide_test_manager_initiable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  IdeTestManager *self = (IdeTestManager *)initable;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_TEST_MANAGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_TEST_PROVIDER,
                                            "context", context,
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

/**
 * ide_test_manager_run_all_async:
 * @self: An #IdeTestManager
 * @cancellable: (nullable): A #GCancellable or %NULL
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
 * Since: 3.28
 */
void
ide_test_manager_run_all_async (IdeTestManager      *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_test_manager_run_all_async);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_test_manager_run_all_finish:
 * @self: An #IdeTestManager
 * @result: A #GAsyncResult
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
 * Since: 3.28
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

/**
 * ide_test_manager_run_async:
 * @self: An #IdeTestManager
 * @test: An #IdeTest
 * @cancellable: (nullable): A #GCancellable, or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Executes a single unit test, asynchronously.
 *
 * The caller can access the result of the operation from @callback
 * by calling ide_test_manager_run_finish() with the provided result.
 *
 * Since: 3.28
 */
void
ide_test_manager_run_async (IdeTestManager      *self,
                            IdeTest             *test,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST_MANAGER (self));
  g_return_if_fail (IDE_IS_TEST (test));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_test_manager_run_async);

  g_task_return_boolean (task, TRUE);

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
 * Since: 3.28
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
