/* ide-symbol-tree.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-symbol-tree"

#include "config.h"

#include "ide-symbol-node.h"
#include "ide-symbol-tree.h"

G_DEFINE_INTERFACE (IdeSymbolTree, ide_symbol_tree, G_TYPE_OBJECT)

static void
ide_symbol_tree_default_init (IdeSymbolTreeInterface *iface)
{
}

/**
 * ide_symbol_tree_get_n_children:
 * @self: An @IdeSymbolTree
 * @node: (allow-none): An #IdeSymbolNode or %NULL.
 *
 * Get the number of children of @node. If @node is NULL, the root node
 * is assumed.
 *
 * Returns: An unsigned integer containing the number of children.
 */
guint
ide_symbol_tree_get_n_children (IdeSymbolTree *self,
                                IdeSymbolNode *node)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_TREE (self), 0);
  g_return_val_if_fail (!node || IDE_IS_SYMBOL_NODE (node), 0);

  return IDE_SYMBOL_TREE_GET_IFACE (self)->get_n_children (self, node);
}

/**
 * ide_symbol_tree_get_nth_child:
 * @self: An #IdeSymbolTree.
 * @node: (allow-none): an #IdeSymboNode
 * @nth: the nth child to retrieve.
 *
 * Gets the @nth child node of @node.
 *
 * Returns: (transfer full) (nullable): an #IdeSymbolNode or %NULL.
 */
IdeSymbolNode *
ide_symbol_tree_get_nth_child (IdeSymbolTree *self,
                               IdeSymbolNode *node,
                               guint          nth)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_TREE (self), NULL);
  g_return_val_if_fail (!node || IDE_IS_SYMBOL_NODE (node), NULL);

  return IDE_SYMBOL_TREE_GET_IFACE (self)->get_nth_child (self, node, nth);
}
