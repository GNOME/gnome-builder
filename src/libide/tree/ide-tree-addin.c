/* ide-tree-addin.c
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

#define G_LOG_DOMAIN "ide-tree-addin"

#include "config.h"

#include <libide-threading.h>

#include "ide-tree-addin.h"

G_DEFINE_INTERFACE (IdeTreeAddin, ide_tree_addin, G_TYPE_OBJECT)

static void
ide_tree_addin_real_build_children_async (IdeTreeAddin        *self,
                                          IdeTreeNode         *node,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_tree_addin_real_build_children_async);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->build_children)
    IDE_TREE_ADDIN_GET_IFACE (self)->build_children (self, node);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_tree_addin_real_build_children_finish (IdeTreeAddin  *self,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_tree_addin_real_node_dropped_async (IdeTreeAddin        *self,
                                        IdeTreeNode         *drag_node,
                                        IdeTreeNode         *drop_node,
                                        GtkSelectionData    *selection,
                                        GdkDragAction        actions,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_tree_addin_real_node_dropped_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Addin does not support dropping nodes");
}

static gboolean
ide_tree_addin_real_node_dropped_finish (IdeTreeAddin  *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_tree_addin_default_init (IdeTreeAddinInterface *iface)
{
  iface->build_children_async = ide_tree_addin_real_build_children_async;
  iface->build_children_finish = ide_tree_addin_real_build_children_finish;
  iface->node_dropped_async = ide_tree_addin_real_node_dropped_async;
  iface->node_dropped_finish = ide_tree_addin_real_node_dropped_finish;
}

/**
 * ide_tree_addin_build_children_async:
 * @self: a #IdeTreeAddin
 * @node: a #IdeTreeNode
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a #GAsyncReadyCallback or %NULL
 * @user_data: user data for @callback
 *
 * This function is called when building the children of a node. This
 * happens when expanding an node that might have children, or building the
 * root node.
 *
 * You may want to use ide_tree_node_holds() to determine if the node
 * contains an item that you are interested in.
 *
 * This function will call the synchronous form of
 * IdeTreeAddin.build_children() if no asynchronous form is available.
 *
 * Since: 3.32
 */
void
ide_tree_addin_build_children_async (IdeTreeAddin        *self,
                                     IdeTreeNode         *node,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TREE_ADDIN_GET_IFACE (self)->build_children_async (self, node, cancellable, callback, user_data);
}

/**
 * ide_tree_addin_build_children_finish:
 * @self: a #IdeTreeAddin
 * @result: result given to callback in ide_tree_addin_build_children_async()
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_tree_addin_build_children_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_tree_addin_build_children_finish (IdeTreeAddin  *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_TREE_ADDIN_GET_IFACE (self)->build_children_finish (self, result, error);
}

/**
 * ide_tree_addin_build_node:
 * @self: a #IdeTreeAddin
 * @node: a #IdeTreeNode
 *
 * This function is called when preparing a node for display in the tree.
 *
 * Addins should adjust any state on the node that makes sense based on the
 * addin.
 *
 * You may want to use ide_tree_node_holds() to determine if the node
 * contains an item that you are interested in.
 *
 * Since: 3.32
 */
void
ide_tree_addin_build_node (IdeTreeAddin *self,
                           IdeTreeNode  *node)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->build_node)
    IDE_TREE_ADDIN_GET_IFACE (self)->build_node (self, node);
}

/**
 * ide_tree_addin_activated:
 * @self: an #IdeTreeAddin
 * @tree: an #IdeTree
 * @node: an #IdeTreeNode
 *
 * This function is called when a node has been activated in the tree
 * and allows for the addin to perform any necessary operations in response
 * to that.
 *
 * If the addin performs an action based on the activation request, then it
 * should return %TRUE from this function so that no further addins may
 * respond to the action.
 *
 * Returns: %TRUE if the activation was handled, otherwise %FALSE
 *
 * Since: 3.32
 */
gboolean
ide_tree_addin_node_activated (IdeTreeAddin *self,
                               IdeTree      *tree,
                               IdeTreeNode  *node)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TREE (tree), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_activated)
    return IDE_TREE_ADDIN_GET_IFACE (self)->node_activated (self, tree, node);

  return FALSE;
}

void
ide_tree_addin_load (IdeTreeAddin *self,
                     IdeTree      *tree,
                     IdeTreeModel *model)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (IDE_IS_TREE_MODEL (model));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->load)
    IDE_TREE_ADDIN_GET_IFACE (self)->load (self, tree, model);
}

void
ide_tree_addin_unload (IdeTreeAddin *self,
                       IdeTree      *tree,
                       IdeTreeModel *model)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (IDE_IS_TREE_MODEL (model));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->unload)
    IDE_TREE_ADDIN_GET_IFACE (self)->unload (self, tree, model);
}

void
ide_tree_addin_selection_changed (IdeTreeAddin *self,
                                  IdeTreeNode  *selection)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (!selection || IDE_IS_TREE_NODE (selection));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->selection_changed)
    IDE_TREE_ADDIN_GET_IFACE (self)->selection_changed (self, selection);
}

void
ide_tree_addin_node_expanded (IdeTreeAddin *self,
                              IdeTreeNode  *node)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_expanded)
    IDE_TREE_ADDIN_GET_IFACE (self)->node_expanded (self, node);
}

void
ide_tree_addin_node_collapsed (IdeTreeAddin *self,
                               IdeTreeNode  *node)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_collapsed)
    IDE_TREE_ADDIN_GET_IFACE (self)->node_collapsed (self, node);
}

gboolean
ide_tree_addin_node_draggable (IdeTreeAddin *self,
                               IdeTreeNode  *node)
{
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_draggable)
    return IDE_TREE_ADDIN_GET_IFACE (self)->node_draggable (self, node);

  return FALSE;
}

gboolean
ide_tree_addin_node_droppable (IdeTreeAddin     *self,
                               IdeTreeNode      *drag_node,
                               IdeTreeNode      *drop_node,
                               GtkSelectionData *selection)
{
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), FALSE);
  g_return_val_if_fail (!drag_node || IDE_IS_TREE_NODE (drag_node), FALSE);
  g_return_val_if_fail (!drop_node || IDE_IS_TREE_NODE (drop_node), FALSE);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_droppable)
    return IDE_TREE_ADDIN_GET_IFACE (self)->node_droppable (self, drag_node, drop_node, selection);

  return FALSE;
}

void
ide_tree_addin_node_dropped_async (IdeTreeAddin        *self,
                                   IdeTreeNode         *drag_node,
                                   IdeTreeNode         *drop_node,
                                   GtkSelectionData    *selection,
                                   GdkDragAction        actions,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (!drag_node || IDE_IS_TREE_NODE (drag_node));
  g_return_if_fail (!drop_node || IDE_IS_TREE_NODE (drop_node));
  g_return_if_fail (selection != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TREE_ADDIN_GET_IFACE (self)->node_dropped_async (self,
                                                       drag_node,
                                                       drop_node,
                                                       selection,
                                                       actions,
                                                       cancellable,
                                                       callback,
                                                       user_data);
}

gboolean
ide_tree_addin_node_dropped_finish (IdeTreeAddin  *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_TREE_ADDIN_GET_IFACE (self)->node_dropped_finish (self, result, error);
}

void
ide_tree_addin_cell_data_func (IdeTreeAddin    *self,
                               IdeTreeNode     *node,
                               GtkCellRenderer *cell)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->cell_data_func)
    IDE_TREE_ADDIN_GET_IFACE (self)->cell_data_func (self, node, cell);
}
