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

void
gbp_project_tree_expand_files (GbpProjectTree *self)
{
  IdeTreeNode *root;
  IdeTreeNode *node = NULL;

  g_return_if_fail (GBP_IS_PROJECT_TREE (self));

  if (!(root = ide_tree_get_root (IDE_TREE (self))))
    return;

  ide_tree_node_traverse (root,
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          1,
                          locate_project_files,
                          &node);

  if (node != NULL)
    ide_tree_expand_node (IDE_TREE (self), node);
}

static void
gbp_project_tree_expand_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GbpProjectTree *self = (GbpProjectTree *)object;
  g_autoptr(IdeTreeNode) root = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TREE_NODE (root));

  if (ide_tree_expand_node_finish (IDE_TREE (self), result, &error))
    gbp_project_tree_expand_files (self);
}

static void
gbp_project_tree_context_set (GtkWidget  *widget,
                              IdeContext *context)
{
  GbpProjectTree *self = (GbpProjectTree *)widget;
  g_autoptr(IdeTreeNode) root = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  root = ide_tree_node_new ();
  ide_tree_node_set_item (root, context);
  ide_tree_set_root (IDE_TREE (self), root);

  ide_tree_expand_node_async (IDE_TREE (self),
                              root,
                              NULL,
                              gbp_project_tree_expand_cb,
                              g_object_ref (root));
}

static void
gbp_project_tree_class_init (GbpProjectTreeClass *klass)
{
}

static void
gbp_project_tree_init (GbpProjectTree *self)
{
  ide_widget_set_context_handler (GTK_WIDGET (self),
                                  gbp_project_tree_context_set);
}

static IdeTreeNode *
gbp_project_tree_get_project_files (GbpProjectTree *self)
{
  IdeTreeNode *project_files = NULL;

  g_assert (GBP_IS_PROJECT_TREE (self));

  ide_tree_node_traverse (ide_tree_get_root (IDE_TREE (self)),
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
  IdeTree *tree = (IdeTree *)object;
  Reveal *r = user_data;

  g_assert (IDE_IS_TREE (tree));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (r != NULL);
  g_assert (GBP_IS_PROJECT_TREE (r->tree));
  g_assert (IDE_IS_TREE_NODE (r->node));
  g_assert (G_IS_FILE (r->file));

  if (!ide_tree_expand_node_finish (tree, result, NULL))
    {
      reveal_free (r);
      return;
    }

  for (IdeTreeNode *child = ide_tree_node_get_first_child (r->node);
       child != NULL;
       child = ide_tree_node_get_next_sibling (child))
    {
      IdeProjectFile *pf;
      g_autoptr(GFile) file = NULL;

      if (ide_tree_node_holds (child, IDE_TYPE_PROJECT_FILE) &&
          (pf = ide_tree_node_get_item (child)) &&
          IDE_IS_PROJECT_FILE (pf) &&
          (file = ide_project_file_ref_file (pf)))
      {
        if (g_file_has_prefix (r->file, file))
          {
            g_set_object (&r->node, child);
            reveal_next (r);
            return;
          }
        else if (g_file_equal (r->file, file))
          {
            ide_tree_set_selected_node (IDE_TREE (r->tree), child);
            gtk_widget_grab_focus (GTK_WIDGET (r->tree));
            break;
          }
      }
    }

  reveal_free (r);
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
      /* If this node cannot have children, then there is no way we
       * can expect to find the child there.
       */
      if (!ide_tree_node_get_children_possible (r->node))
        goto failure;

      ide_tree_expand_node_async (IDE_TREE (r->tree),
                                  r->node,
                                  NULL,
                                  reveal_next_cb,
                                  r);

      return;
    }
  else if (g_file_equal (r->file, file))
    {
      ide_tree_set_selected_node (IDE_TREE (r->tree), r->node);
      gtk_widget_grab_focus (GTK_WIDGET (r->tree));
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
