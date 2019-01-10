/* gbp-project-tree.c
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

#define G_LOG_DOMAIN "gbp-project-tree"

#include "config.h"

#include <libide-gui.h>
#include <libide-projects.h>

#include "gbp-project-tree.h"

struct _GbpProjectTree
{
  IdeTree parent_instance;
};

G_DEFINE_TYPE (GbpProjectTree, gbp_project_tree, IDE_TYPE_TREE)

static IdeTreeNodeVisit
locate_project_files (IdeTreeNode *node,
                      gpointer     user_data)
{
  IdeTreeNode **out_node = user_data;

  if (ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    {
      *out_node = node;
      return IDE_TREE_NODE_VISIT_BREAK;
    }

  return IDE_TREE_NODE_VISIT_CONTINUE;
}

static void
project_files_expanded_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeTreeModel *model = (IdeTreeModel *)object;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_tree_model_expand_finish (model, result, NULL))
    {
      g_autoptr(GtkTreePath) path = NULL;
      GbpProjectTree *self;
      IdeTreeNode *node;

      self = ide_task_get_source_object (task);
      node = ide_task_get_task_data (task);

      g_assert (GBP_IS_PROJECT_TREE (self));
      g_assert (IDE_IS_TREE_NODE (node));

      if ((path = ide_tree_node_get_path (node)))
        gtk_tree_view_expand_row (GTK_TREE_VIEW (self), path, FALSE);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_project_tree_expand_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeTreeModel *model = (IdeTreeModel *)object;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_tree_model_expand_finish (model, result, NULL))
    {
      IdeTreeNode *root = ide_tree_model_get_root (model);
      IdeTreeNode *node = NULL;

      ide_tree_node_traverse (root,
                              G_PRE_ORDER,
                              G_TRAVERSE_ALL,
                              1,
                              locate_project_files,
                              &node);

      if (node == NULL)
        goto cleanup;

      ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

      ide_tree_model_expand_async (model,
                                   node,
                                   NULL,
                                   project_files_expanded_cb,
                                   g_steal_pointer (&task));

      return;
    }

cleanup:
  ide_task_return_boolean (task, TRUE);
}

static void
gbp_project_tree_hierarchy_changed (GtkWidget *widget,
                                    GtkWidget *old_toplevel)
{
  GbpProjectTree *self = (GbpProjectTree *)widget;
  GtkWidget *toplevel;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE (self));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

  if (IDE_IS_WORKSPACE (toplevel))
    {
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (toplevel));
      g_autoptr(IdeTreeNode) root = ide_tree_node_new ();
      g_autoptr(IdeTreeModel) model = NULL;
      g_autoptr(IdeTask) task = NULL;

      model = g_object_new (IDE_TYPE_TREE_MODEL,
                            "kind", "project-tree",
                            "tree", self,
                            NULL);
      gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (model));

      ide_tree_node_set_item (root, context);
      ide_object_append (IDE_OBJECT (context), IDE_OBJECT (model));
      ide_tree_model_set_root (model, root);

      task = ide_task_new (self, NULL, NULL, NULL);
      ide_task_set_source_tag (task, gbp_project_tree_hierarchy_changed);

      ide_tree_model_expand_async (model,
                                   root,
                                   NULL,
                                   gbp_project_tree_expand_cb,
                                   g_steal_pointer (&task));
    }
}

static void
gbp_project_tree_class_init (GbpProjectTreeClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->hierarchy_changed = gbp_project_tree_hierarchy_changed;
}

static void
gbp_project_tree_init (GbpProjectTree *self)
{
}
