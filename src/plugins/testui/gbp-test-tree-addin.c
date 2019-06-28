/* gbp-test-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-test-tree-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-tree.h>
#include <libide-threading.h>

#include "ide-tree-private.h"

#include "gbp-test-path.h"
#include "gbp-test-output-panel.h"
#include "gbp-test-tree-addin.h"

struct _GbpTestTreeAddin
{
  GObject             parent_instance;
  IdeTreeModel       *model;
  IdeTree            *tree;
  GbpTestOutputPanel *panel;
};

typedef struct
{
  IdeTreeNode     *node;
  IdeTest         *test;
  IdeNotification *notif;
} RunTest;

static void
run_test_free (RunTest *state)
{
  g_clear_object (&state->node);
  g_clear_object (&state->test);
  g_clear_object (&state->notif);
  g_slice_free (RunTest, state);
}

static void
show_test_panel (GbpTestTreeAddin *self)
{
  IdeTestManager *test_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TEST_TREE_ADDIN (self));

  if (!(context = ide_object_get_context (IDE_OBJECT (self->model))) ||
      !ide_context_has_project (context) ||
      !(test_manager = ide_test_manager_from_context (context)))
    return;

  if (self->panel == NULL)
    {
      GtkWidget *surface;
      GtkWidget *utils;
      VtePty *pty;

      pty = ide_test_manager_get_pty (test_manager);
      self->panel = GBP_TEST_OUTPUT_PANEL (gbp_test_output_panel_new (pty));
      g_signal_connect (self->panel,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->panel);
      surface = gtk_widget_get_ancestor (GTK_WIDGET (self->tree), IDE_TYPE_EDITOR_SURFACE);
      utils = ide_editor_surface_get_utilities (IDE_EDITOR_SURFACE (surface));
      gtk_container_add (GTK_CONTAINER (utils), GTK_WIDGET (self->panel));
      gtk_widget_show (GTK_WIDGET (self->panel));
      dzl_dock_item_present (DZL_DOCK_ITEM (self->panel));
    }
}

static void
gbp_test_tree_addin_build_paths_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeTestManager *test_manager = (IdeTestManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) dirs = NULL;
  g_autoptr(GPtrArray) tests = NULL;
  IdeTreeNode *node;
  GbpTestPath *path;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (test_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ide_test_manager_ensure_loaded_finish (test_manager, result, NULL);

  node = ide_task_get_task_data (task);
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (ide_tree_node_holds (node, GBP_TYPE_TEST_PATH));

  path = ide_tree_node_get_item (node);
  g_assert (GBP_IS_TEST_PATH (path));

  dirs = gbp_test_path_get_folders (path);
  tests = gbp_test_path_get_tests (path);

  IDE_PTR_ARRAY_SET_FREE_FUNC (dirs, g_object_unref);
  IDE_PTR_ARRAY_SET_FREE_FUNC (tests, g_object_unref);

  for (guint i = 0; i < dirs->len; i++)
    {
      GbpTestPath *child_path = g_ptr_array_index (dirs, i);
      g_autoptr(IdeTreeNode) child = NULL;

      child = ide_tree_node_new ();
      ide_tree_node_set_children_possible (child, TRUE);
      ide_tree_node_set_display_name (child, gbp_test_path_get_name (child_path));
      ide_tree_node_set_icon_name (child, "folder-symbolic");
      ide_tree_node_set_expanded_icon_name (child, "folder-open-symbolic");
      ide_tree_node_set_item (child, child_path);
      ide_tree_node_append (node, child);
    }

  for (guint i = 0; i < tests->len; i++)
    {
      IdeTest *test = g_ptr_array_index (tests, i);
      g_autoptr(IdeTreeNode) child = NULL;

      child = ide_tree_node_new ();
      ide_tree_node_set_children_possible (child, FALSE);
      ide_tree_node_set_display_name (child, ide_test_get_display_name (test));
      ide_tree_node_set_icon_name (child, ide_test_get_icon_name (test));
      ide_tree_node_set_item (child, test);
      ide_tree_node_append (node, child);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_test_tree_addin_build_children_async (IdeTreeAddin        *addin,
                                          IdeTreeNode         *node,
                                          GCancellable        *cancellbale,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GbpTestTreeAddin *self = (GbpTestTreeAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TEST_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellbale || G_IS_CANCELLABLE (cancellbale));

  task = ide_task_new (addin, cancellbale, callback, user_data);
  ide_task_set_source_tag (task, gbp_test_tree_addin_build_children_async);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self->model));

  if (!ide_context_has_project (context))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  if (ide_tree_node_holds (node, IDE_TYPE_CONTEXT))
    {
      g_autoptr(IdeTreeNode) child = NULL;
      g_autoptr(GbpTestPath) path = NULL;
      IdeTestManager *test_manager;

      test_manager = ide_test_manager_from_context (context);
      path = gbp_test_path_new (test_manager, NULL);

      child = ide_tree_node_new ();
      ide_tree_node_set_children_possible (child, TRUE);
      ide_tree_node_set_display_name (child, _("Unit Tests"));
      ide_tree_node_set_icon_name (child, "builder-unit-tests-symbolic");
      ide_tree_node_set_is_header (child, TRUE);
      ide_tree_node_set_item (child, path);
      ide_tree_node_prepend (node, child);
    }
  else if (ide_tree_node_holds (node, GBP_TYPE_TEST_PATH))
    {
      IdeTestManager *test_manager = ide_test_manager_from_context (context);

      ide_test_manager_ensure_loaded_async (test_manager,
                                            NULL,
                                            gbp_test_tree_addin_build_paths_cb,
                                            g_steal_pointer (&task));
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_test_tree_addin_build_children_finish (IdeTreeAddin  *addin,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TEST_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static IdeTreeNodeVisit
locate_unit_tests (IdeTreeNode *node,
                   gpointer     user_data)
{
  IdeTreeNode **out_node = user_data;

  if (ide_tree_node_holds (node, GBP_TYPE_TEST_PATH))
    {
      *out_node = node;
      return IDE_TREE_NODE_VISIT_BREAK;
    }

  return IDE_TREE_NODE_VISIT_CONTINUE;
}

static void
gbp_test_tree_addin_notify_loading (GbpTestTreeAddin *self,
                                    GParamSpec       *pspec,
                                    IdeTestManager   *test_manager)
{
  IdeTreeNode *root;
  IdeTreeNode *node = NULL;
  gint64 loading_time;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TEST_TREE_ADDIN (self));
  g_assert (IDE_IS_TEST_MANAGER (test_manager));

  root = ide_tree_model_get_root (self->model);

  ide_tree_node_traverse (root,
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          1,
                          locate_unit_tests,
                          &node);

  if (node != NULL &&
      ide_tree_node_expanded (self->tree, node) &&
      !_ide_tree_node_get_loading (node, &loading_time))
    {
      ide_tree_collapse_node (self->tree, node);
      ide_tree_expand_node (self->tree, node);
    }
}

static void
gbp_test_tree_addin_load (IdeTreeAddin *addin,
                          IdeTree      *tree,
                          IdeTreeModel *model)
{
  GbpTestTreeAddin *self = (GbpTestTreeAddin *)addin;
  IdeTestManager *test_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->tree = tree;
  self->model = model;

  context = ide_object_get_context (IDE_OBJECT (model));

  if (!ide_context_has_project (context))
    return;

  test_manager = ide_test_manager_from_context (context);

  g_signal_connect_object (test_manager,
                           "notify::loading",
                           G_CALLBACK (gbp_test_tree_addin_notify_loading),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_test_tree_addin_unload (IdeTreeAddin *addin,
                            IdeTree      *tree,
                            IdeTreeModel *model)
{
  GbpTestTreeAddin *self = (GbpTestTreeAddin *)addin;
  IdeTestManager *test_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->tree = NULL;
  self->model = NULL;

  if (self->panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->panel));

  context = ide_object_get_context (IDE_OBJECT (model));

  if (!ide_context_has_project (context))
    return;

  test_manager = ide_test_manager_from_context (context);
  g_signal_handlers_disconnect_by_func (test_manager,
                                        G_CALLBACK (gbp_test_tree_addin_notify_loading),
                                        self);
}

static void
gbp_test_tree_addin_run_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeTestManager *test_manager = (IdeTestManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const gchar *icon_name = NULL;
  RunTest *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST_MANAGER (test_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_test_manager_run_finish (test_manager, result, &error))
    {
      /* TODO: Plumb more errors into test-manager */
      if (g_error_matches (error, IDE_RUNTIME_ERROR, IDE_RUNTIME_ERROR_BUILD_FAILED) ||
          error->domain == G_SPAWN_ERROR)
        icon_name = "dialog-warning-symbolic";
    }

  state = ide_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));
  g_assert (IDE_IS_TEST (state->test));
  g_assert (IDE_IS_NOTIFICATION (state->notif));

  if (icon_name == NULL)
    icon_name = ide_test_get_icon_name (state->test);
  ide_tree_node_set_icon_name (state->node, icon_name);

  ide_notification_withdraw_in_seconds (state->notif, 1);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_test_tree_addin_node_activated (IdeTreeAddin *addin,
                                    IdeTree      *tree,
                                    IdeTreeNode  *node)
{
  GbpTestTreeAddin *self = (GbpTestTreeAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *title = NULL;
  IdeTestManager *test_manager;
  IdeContext *context;
  RunTest *state;
  IdeTest *test;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TEST_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!ide_tree_node_holds (node, IDE_TYPE_TEST))
    return FALSE;

  context = ide_widget_get_context (GTK_WIDGET (tree));
  test_manager = ide_test_manager_from_context (context);
  test = ide_tree_node_get_item (node);

  state = g_slice_new0 (RunTest);
  state->node = g_object_ref (node);
  state->test = g_object_ref (test);
  state->notif = ide_notification_new ();

  /* translators: %s is replaced with the name of the unit test */
  title = g_strdup_printf (_("Running test “%s”…"),
                           ide_test_get_display_name (test));
  ide_notification_set_title (state->notif, title);
  ide_notification_set_urgent (state->notif, TRUE);
  ide_notification_attach (state->notif, IDE_OBJECT (context));

  task = ide_task_new (addin, NULL, NULL, NULL);
  ide_task_set_source_tag (task, gbp_test_tree_addin_node_activated);
  ide_task_set_task_data (task, state, run_test_free);

  ide_tree_node_set_icon_name (node, "content-loading-symbolic");

  show_test_panel (self);

  ide_test_manager_run_async (test_manager,
                              test,
                              ide_test_manager_get_cancellable (test_manager),
                              gbp_test_tree_addin_run_cb,
                              g_steal_pointer (&task));

  return TRUE;
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->load = gbp_test_tree_addin_load;
  iface->unload = gbp_test_tree_addin_unload;
  iface->build_children_async = gbp_test_tree_addin_build_children_async;
  iface->build_children_finish = gbp_test_tree_addin_build_children_finish;
  iface->node_activated = gbp_test_tree_addin_node_activated;
}

G_DEFINE_TYPE_WITH_CODE (GbpTestTreeAddin, gbp_test_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_test_tree_addin_class_init (GbpTestTreeAddinClass *klass)
{
}

static void
gbp_test_tree_addin_init (GbpTestTreeAddin *self)
{
}
