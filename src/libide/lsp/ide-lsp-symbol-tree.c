/* ide-lsp-symbol-tree.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-symbol-tree"

#include "config.h"

#include "ide-lsp-symbol-node.h"
#include "ide-lsp-symbol-node-private.h"
#include "ide-lsp-symbol-tree.h"
#include "ide-lsp-symbol-tree-private.h"

typedef struct
{
  GPtrArray *symbols;
  GNode      root;
} IdeLspSymbolTreePrivate;

static void symbol_tree_iface_init (IdeSymbolTreeInterface *iface);

struct _IdeLspSymbolTree { GObject object; };
G_DEFINE_FINAL_TYPE_WITH_CODE (IdeLspSymbolTree, ide_lsp_symbol_tree, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeLspSymbolTree)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

static guint
ide_lsp_symbol_tree_get_n_children (IdeSymbolTree *tree,
                                         IdeSymbolNode *parent)
{
  IdeLspSymbolTree *self = (IdeLspSymbolTree *)tree;
  IdeLspSymbolTreePrivate *priv = ide_lsp_symbol_tree_get_instance_private (self);

  g_assert (IDE_IS_LSP_SYMBOL_TREE (self));
  g_assert (!parent || IDE_IS_LSP_SYMBOL_NODE (parent));

  if (parent == NULL)
    return g_node_n_children (&priv->root);

  return g_node_n_children (&IDE_LSP_SYMBOL_NODE (parent)->gnode);
}

static IdeSymbolNode *
ide_lsp_symbol_tree_get_nth_child (IdeSymbolTree *tree,
                                        IdeSymbolNode *parent,
                                        guint          nth)
{
  IdeLspSymbolTree *self = (IdeLspSymbolTree *)tree;
  IdeLspSymbolTreePrivate *priv = ide_lsp_symbol_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_SYMBOL_TREE (self), NULL);
  g_return_val_if_fail (!parent || IDE_IS_LSP_SYMBOL_NODE (parent), NULL);

  if (parent == NULL)
    {
      g_return_val_if_fail (nth < g_node_n_children (&priv->root), NULL);
      return g_object_ref (g_node_nth_child (&priv->root, nth)->data);
    }

  g_return_val_if_fail (nth < g_node_n_children (&IDE_LSP_SYMBOL_NODE (parent)->gnode), NULL);
  return g_object_ref (g_node_nth_child (&IDE_LSP_SYMBOL_NODE (parent)->gnode, nth)->data);
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_lsp_symbol_tree_get_n_children;
  iface->get_nth_child = ide_lsp_symbol_tree_get_nth_child;
}

static void
ide_lsp_symbol_tree_finalize (GObject *object)
{
  IdeLspSymbolTree *self = (IdeLspSymbolTree *)object;
  IdeLspSymbolTreePrivate *priv = ide_lsp_symbol_tree_get_instance_private (self);

  g_clear_pointer (&priv->symbols, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_lsp_symbol_tree_parent_class)->finalize (object);
}

static void
ide_lsp_symbol_tree_class_init (IdeLspSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_symbol_tree_finalize;
}

static void
ide_lsp_symbol_tree_init (IdeLspSymbolTree *self)
{
}

static void
add_to_node (GNode                 *node,
             IdeLspSymbolNode *symbol)
{
  /* First, check to see if any of the children are parents of the range of
   * this symbol. If so, we will defer to adding it to that node.
   */

  for (GNode *iter = node->children; iter != NULL; iter = iter->next)
    {
      IdeLspSymbolNode *child = iter->data;

      /*
       * If this node is an ancestor of ours, then we can defer to
       * adding to that node.
       */
      if (ide_lsp_symbol_node_is_parent_of (child, symbol))
        {
          add_to_node (iter, symbol);
          return;
        }

      /*
       * If we are the parent of the child, then we need to insert
       * ourselves in its place and add it to our node.
       */
      if (ide_lsp_symbol_node_is_parent_of (symbol, child))
        {
          /* Add this node to our children */
          g_node_unlink (&child->gnode);
          g_node_append (&symbol->gnode, &child->gnode);

          /* add ourselves to the tree at this level */
          g_node_append (node, &symbol->gnode);

          return;
        }
    }

  g_node_append (node, &symbol->gnode);
}

static void
ide_lsp_symbol_tree_build (IdeLspSymbolTree *self)
{
  IdeLspSymbolTreePrivate *priv = ide_lsp_symbol_tree_get_instance_private (self);

  g_assert (IDE_IS_LSP_SYMBOL_TREE (self));
  g_assert (priv->symbols != NULL);

  for (guint i = 0; i < priv->symbols->len; i++)
    add_to_node (&priv->root, g_ptr_array_index (priv->symbols, i));
}

/**
 * ide_lsp_symbol_tree_new:
 * @symbols: (transfer full) (element-type Ide.LspSymbolNode): The symbols
 *
 * Creates a new #IdeLspSymbolTree but takes ownership of @ar.
 *
 * Returns: (transfer full): A newly allocated #IdeLspSymbolTree.
 */
IdeLspSymbolTree *
ide_lsp_symbol_tree_new (GPtrArray *symbols)
{
  IdeLspSymbolTreePrivate *priv;
  IdeLspSymbolTree *self;

  g_return_val_if_fail (symbols != NULL, NULL);

  IDE_PTR_ARRAY_SET_FREE_FUNC (symbols, g_object_unref);

  self = g_object_new (IDE_TYPE_LSP_SYMBOL_TREE, NULL);
  priv = ide_lsp_symbol_tree_get_instance_private (self);
  priv->symbols = symbols;

  ide_lsp_symbol_tree_build (self);

  return self;
}
