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

#include "ide-tree-private.h"

#include "gbp-project-tree.h"

struct _GbpProjectTree
{
  IdeTree parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpProjectTree, gbp_project_tree, IDE_TYPE_TREE)

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

static IdeTreeNode *
gbp_project_tree_get_project_files (GbpProjectTree *self)
{
  IdeTreeModel *model;
  IdeTreeNode *project_files = NULL;

  g_assert (GBP_IS_PROJECT_TREE (self));

  model = IDE_TREE_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (self)));
  ide_tree_node_traverse (ide_tree_model_get_root (model),
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          1,
                          locate_project_files,
                          &project_files);

  return project_files;
}

typedef struct
{
  GbpProjectTree *tree;
  IdeTreeNode    *node;
  GFile          *file;
} Reveal;

static void reveal_next (Reveal *r);

static void
reveal_free (Reveal *r)
{
  g_clear_object (&r->tree);
  g_clear_object (&r->node);
  g_clear_object (&r->file);
  g_free (r);
}

static void
reveal_next_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  IdeTreeModel *model = (IdeTreeModel *)object;
  Reveal *r = user_data;

  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (r != NULL);
  g_assert (GBP_IS_PROJECT_TREE (r->tree));
  g_assert (IDE_IS_TREE_NODE (r->node));
  g_assert (G_IS_FILE (r->file));

  if (!ide_tree_model_expand_finish (model, result, NULL))
    reveal_free (r);
  else
    reveal_next (g_steal_pointer (&r));
}

static void
reveal_next (Reveal *r)
{
  g_autoptr(GFile) file = NULL;
  IdeProjectFile *pf;

  g_assert (r != NULL);
  g_assert (GBP_IS_PROJECT_TREE (r->tree));
  g_assert (IDE_IS_TREE_NODE (r->node));
  g_assert (G_IS_FILE (r->file));

  if (!ide_tree_node_holds (r->node, IDE_TYPE_PROJECT_FILE) ||
      !(pf = ide_tree_node_get_item (r->node)) ||
      !IDE_IS_PROJECT_FILE (pf) ||
      !(file = ide_project_file_ref_file (pf)))
    goto failure;

  if (g_file_has_prefix (r->file, file))
    {
      IdeTreeNode *child;

      /* If this node cannot have children, then there is no way we
       * can expect to find the child there.
       */
      if (!ide_tree_node_get_children_possible (r->node))
        goto failure;

      /* If this node needs to be built, then build it before we
       * continue processing.
       */
      if (_ide_tree_node_get_needs_build_children (r->node))
        {
          IdeTreeModel *model;

          if (!(model = IDE_TREE_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (r->tree)))))
            goto failure;

          ide_tree_model_expand_async (model,
                                       r->node,
                                       NULL,
                                       reveal_next_cb,
                                       r);
          return;
        }

      /* Tree to find the first child which is equal to or is a prefix
       * for the target file.
       */
      if (!(child = ide_tree_node_get_nth_child (r->node, 0)))
        goto failure;

      do
        {
          IdeProjectFile *cpf;
          g_autoptr(GFile) cf = NULL;

          if (!ide_tree_node_holds (child, IDE_TYPE_PROJECT_FILE) ||
              !(cpf = ide_tree_node_get_item (child)) ||
              !IDE_IS_PROJECT_FILE (cpf) ||
              !(cf = ide_project_file_ref_file (cpf)) ||
              !G_IS_FILE (cf))
            continue;

          if (g_file_has_prefix (r->file, cf) || g_file_equal (r->file, cf))
            {
              g_set_object (&r->node, child);
              reveal_next (r);
              return;
            }
        }
      while ((child = ide_tree_node_get_next (child)));
    }
  else if (g_file_equal (r->file, file))
    {
      g_autoptr(GtkTreePath) path = ide_tree_node_get_path (r->node);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (r->tree), path);
      ide_tree_select_node (IDE_TREE (r->tree), r->node);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (r->tree),
                                    path, NULL, FALSE, 0, 0);

      /* Due to the way tree views work, we need to use this function to
       * grab keyboard focus on a particular row in the treeview. This allows
       * pressing the menu key and navigate with the arrow keys in the tree
       * view without leaving the keyboard :)
       */
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (r->tree),
                                path,
                                NULL,
                                FALSE);
      /* We still need to grab the focus on the tree view widget as suggested
       * by the documentation. ide_widget_reveal_and_grab() also makes the left
       * dock show up automatically which is very nice because it avoids having
       * to press F9 when it could have been revealed automatically.
       */
      ide_widget_reveal_and_grab (GTK_WIDGET (r->tree));
    }

failure:
  reveal_free (r);
}

void
gbp_project_tree_reveal (GbpProjectTree *self,
                         GFile          *file)
{
  IdeTreeNode *project_files;
  Reveal *r;

  g_return_if_fail (GBP_IS_PROJECT_TREE (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (file == NULL)
    return;

  project_files = gbp_project_tree_get_project_files (self);

  if (!IDE_IS_TREE_NODE (project_files))
    return;

  r = g_new0 (Reveal, 1);
  r->tree = g_object_ref (self);
  r->node = g_object_ref (project_files);
  r->file = g_object_ref (file);

  reveal_next (g_steal_pointer (&r));
}
