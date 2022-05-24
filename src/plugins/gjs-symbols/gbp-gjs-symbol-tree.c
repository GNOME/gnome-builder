/* gbp-gjs-symbol-tree.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gjs-symbol-tree"

#include "config.h"

#include <libide-code.h>

#include "gbp-gjs-symbol-tree.h"

struct _GbpGjsSymbolTree
{
  GObject           parent_instance;
  GbpGjsSymbolNode *root;
};

static guint
gbp_gjs_symbol_tree_get_n_children (IdeSymbolTree *tree,
                                    IdeSymbolNode *node)
{
  GbpGjsSymbolTree *self = (GbpGjsSymbolTree *)tree;

  g_assert (GBP_IS_GJS_SYMBOL_TREE (self));
  g_assert (!node || GBP_IS_GJS_SYMBOL_NODE (node));

  if (node == NULL)
    return gbp_gjs_symbol_node_get_n_children (self->root);
  else
    return gbp_gjs_symbol_node_get_n_children (GBP_GJS_SYMBOL_NODE (node));
}

static IdeSymbolNode *
gbp_gjs_symbol_tree_get_nth_child (IdeSymbolTree *tree,
                                   IdeSymbolNode *node,
                                   guint          nth_child)
{
  GbpGjsSymbolTree *self = (GbpGjsSymbolTree *)tree;

  g_assert (GBP_IS_GJS_SYMBOL_TREE (self));
  g_assert (!node || GBP_IS_GJS_SYMBOL_NODE (node));

  if (node == NULL)
    return gbp_gjs_symbol_node_get_nth_child (self->root, nth_child);
  else
    return gbp_gjs_symbol_node_get_nth_child (GBP_GJS_SYMBOL_NODE (node), nth_child);
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = gbp_gjs_symbol_tree_get_n_children;
  iface->get_nth_child = gbp_gjs_symbol_tree_get_nth_child;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGjsSymbolTree, gbp_gjs_symbol_tree, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

static void
gbp_gjs_symbol_tree_finalize (GObject *object)
{
  GbpGjsSymbolTree *self = (GbpGjsSymbolTree *)object;

  g_clear_object (&self->root);

  G_OBJECT_CLASS (gbp_gjs_symbol_tree_parent_class)->finalize (object);
}

static void
gbp_gjs_symbol_tree_class_init (GbpGjsSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gjs_symbol_tree_finalize;
}

static void
gbp_gjs_symbol_tree_init (GbpGjsSymbolTree *self)
{
}

GbpGjsSymbolTree *
gbp_gjs_symbol_tree_new (GbpGjsSymbolNode *root)
{
  GbpGjsSymbolTree *self;

  g_return_val_if_fail (GBP_IS_GJS_SYMBOL_NODE (root), NULL);

  self = g_object_new (GBP_TYPE_GJS_SYMBOL_TREE, NULL);
  self->root = g_object_ref (root);

  return self;
}
