/* ide-tree-addin.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

#include "ide-tree-addin.h"

G_DEFINE_INTERFACE (IdeTreeAddin, ide_tree_addin, G_TYPE_OBJECT)

static void
ide_tree_addin_real_build_children_async (IdeTreeAddin        *self,
                                          IdeTreeNode         *node,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_tree_addin_real_build_children_async);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->build_children)
    IDE_TREE_ADDIN_GET_IFACE (self)->build_children (self, node);

  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_tree_addin_real_build_children_finish (IdeTreeAddin  *self,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_tree_addin_real_node_dropped_async (IdeTreeAddin        *self,
                                        GtkDropTarget       *drop_target,
                                        IdeTreeNode         *drop_node,
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
                     IdeTree      *tree)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->load)
    IDE_TREE_ADDIN_GET_IFACE (self)->load (self, tree);
}

void
ide_tree_addin_unload (IdeTreeAddin *self,
                       IdeTree      *tree)
{
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (IDE_IS_TREE (tree));

  if (IDE_TREE_ADDIN_GET_IFACE (self)->unload)
    IDE_TREE_ADDIN_GET_IFACE (self)->unload (self, tree);
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

/**
 * ide_tree_addin_node_draggable:
 * @self: a #IdeTreeAddin
 * @node: an #IdeTreeNode
 *
 * Checks if a node is draggable.
 *
 * Returns: (transfer full) (nullable): %NULL or a #GdkContentProvider if
 *   the node is draggable.
 *
 * Since: 44
 */
GdkContentProvider *
ide_tree_addin_node_draggable (IdeTreeAddin *self,
                               IdeTreeNode  *node)
{
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), NULL);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_draggable)
    return IDE_TREE_ADDIN_GET_IFACE (self)->node_draggable (self, node);

  return NULL;
}

/**
 * ide_tree_addin_node_droppable:
 * @self: an #IdeTreeAddin
 * @drop_target: a #GtkDropTarget
 * @drop_node: an #IdeTreeNode
 * @gtypes: (element-type GType): an array of #GType
 *
 * Determines if @drop_node is a droppable for @drop_target.
 *
 * If so, this function should add the allowed #GType to @gtypes.
 *
 * Returns: 0 if not droppable, otherwise a #GdkDragAction
 *
 * Since: 44
 */
GdkDragAction
ide_tree_addin_node_droppable (IdeTreeAddin  *self,
                               GtkDropTarget *drop_target,
                               IdeTreeNode   *drop_node,
                               GArray        *gtypes)
{
  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), 0);
  g_return_val_if_fail (GTK_IS_DROP_TARGET (drop_target), 0);
  g_return_val_if_fail (IDE_IS_TREE_NODE (drop_node), 0);
  g_return_val_if_fail (gtypes != NULL, 0);

  if (IDE_TREE_ADDIN_GET_IFACE (self)->node_droppable)
    return IDE_TREE_ADDIN_GET_IFACE (self)->node_droppable (self, drop_target, drop_node, gtypes);

  return 0;
}

void
ide_tree_addin_node_dropped_async (IdeTreeAddin        *self,
                                   GtkDropTarget       *drop_target,
                                   IdeTreeNode         *drop_node,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_ADDIN (self));
  g_return_if_fail (!drop_node || IDE_IS_TREE_NODE (drop_node));
  g_return_if_fail (GTK_IS_DROP_TARGET (drop_target));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_TREE_ADDIN_GET_IFACE (self)->node_dropped_async (self, drop_target, drop_node, cancellable, callback, user_data);
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

static void
ide_tree_addin_build_children_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  GError *error = NULL;

  if (!ide_tree_addin_build_children_finish (IDE_TREE_ADDIN (object), result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_boolean (promise, TRUE);
}

/**
 * ide_tree_addin_build_children:
 * @self: a #IdeTreeAddin
 * @node: the node to be built
 *
 * Returns a future which resolves when the children are
 * built or rejects with failure.
 *
 * Returns: (transfer full): a #DexFuture
 */
DexFuture *
ide_tree_addin_build_children (IdeTreeAddin *self,
                               IdeTreeNode  *node)
{
  DexPromise *promise;

  g_return_val_if_fail (IDE_IS_TREE_ADDIN (self), NULL);

  promise = dex_promise_new_cancellable ();

  ide_tree_addin_build_children_async (self,
                                       node,
                                       dex_promise_get_cancellable (promise),
                                       ide_tree_addin_build_children_cb,
                                       dex_ref (promise));

  return DEX_FUTURE (promise);
}
