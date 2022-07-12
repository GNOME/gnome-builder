/* ide-ctags-symbol-tree.c
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

#define G_LOG_DOMAIN "ide-ctags-symbol-tree"

#include "ide-ctags-symbol-node.h"
#include "ide-ctags-symbol-tree.h"

struct _IdeCtagsSymbolTree
{
  GObject parent_instance;
  GPtrArray *ar;
};

static guint
ide_ctags_symbol_tree_get_n_children (IdeSymbolTree *tree,
                                      IdeSymbolNode *node)
{
  IdeCtagsSymbolTree *self = (IdeCtagsSymbolTree *)tree;

  g_assert (IDE_IS_CTAGS_SYMBOL_TREE (tree));
  g_assert (!node || IDE_IS_CTAGS_SYMBOL_NODE (node));

  if (node == NULL)
    return self->ar->len;

  return ide_ctags_symbol_node_get_n_children (IDE_CTAGS_SYMBOL_NODE (node));
}

static IdeSymbolNode *
ide_ctags_symbol_tree_get_nth_child (IdeSymbolTree *tree,
                                     IdeSymbolNode *node,
                                     guint          nth)
{
  IdeCtagsSymbolTree *self = (IdeCtagsSymbolTree *)tree;

  g_assert (IDE_IS_CTAGS_SYMBOL_TREE (tree));
  g_assert (!node || IDE_IS_CTAGS_SYMBOL_NODE (node));

  if (node == NULL)
    {
      if (nth < self->ar->len)
        return g_object_ref (g_ptr_array_index (self->ar, nth));
      return NULL;
    }

  return ide_ctags_symbol_node_get_nth_child (IDE_CTAGS_SYMBOL_NODE (node), nth);
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_ctags_symbol_tree_get_n_children;
  iface->get_nth_child = ide_ctags_symbol_tree_get_nth_child;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCtagsSymbolTree, ide_ctags_symbol_tree, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

/**
 * ide_ctags_symbol_tree_new:
 * @ar: An array of #IdeSymbol instances
 *
 * This function takes ownership of @ar.
 *
 *
 */
IdeCtagsSymbolTree *
ide_ctags_symbol_tree_new (GPtrArray *ar)
{
  IdeCtagsSymbolTree *self;

  self = g_object_new (IDE_TYPE_CTAGS_SYMBOL_TREE, NULL);
  self->ar = ar;

  return self;
}

static void
ide_ctags_symbol_tree_finalize (GObject *object)
{
  IdeCtagsSymbolTree *self = (IdeCtagsSymbolTree *)object;

  g_clear_pointer (&self->ar, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_ctags_symbol_tree_parent_class)->finalize (object);
}

static void
ide_ctags_symbol_tree_class_init (IdeCtagsSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_symbol_tree_finalize;
}

static void
ide_ctags_symbol_tree_init (IdeCtagsSymbolTree *self)
{
}
