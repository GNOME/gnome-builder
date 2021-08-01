/* ide-xml-symbol-tree.c
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-xml-symbol-tree"

#include "ide-xml-symbol-tree.h"

struct _IdeXmlSymbolTree
{
  GObject           parent_instance;

  IdeXmlSymbolNode *root_node;
};

static void symbol_tree_iface_init (IdeSymbolTreeInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeXmlSymbolTree, ide_xml_symbol_tree, G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

static guint
ide_xml_symbol_tree_get_n_children (IdeSymbolTree *tree,
                                    IdeSymbolNode *node)
{
  IdeXmlSymbolTree *self = (IdeXmlSymbolTree *)tree;

  g_assert (IDE_IS_XML_SYMBOL_TREE (tree));
  g_assert (node == NULL || IDE_IS_XML_SYMBOL_NODE (node));

  if (node == NULL)
    node = (IdeSymbolNode *)self->root_node;

  return ide_xml_symbol_node_get_n_children (IDE_XML_SYMBOL_NODE (node));
}

static IdeSymbolNode *
ide_xml_symbol_tree_get_nth_child (IdeSymbolTree *tree,
                                   IdeSymbolNode *node,
                                   guint          nth)
{
  IdeXmlSymbolTree *self = (IdeXmlSymbolTree *)tree;
  guint n_children;

  g_assert (IDE_IS_XML_SYMBOL_TREE (tree));
  g_assert (node == NULL || IDE_IS_XML_SYMBOL_NODE (node));

  if (node == NULL)
    node = IDE_SYMBOL_NODE (self->root_node);

  n_children = ide_xml_symbol_node_get_n_children (IDE_XML_SYMBOL_NODE (node));
  if (nth < n_children)
    return ide_xml_symbol_node_get_nth_child (IDE_XML_SYMBOL_NODE (node), nth);

  g_warning ("nth child %u is out of bounds", nth);

  return NULL;
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_xml_symbol_tree_get_n_children;
  iface->get_nth_child = ide_xml_symbol_tree_get_nth_child;
}

/**
 * ide_xml_symbol_tree_new:
 * @root_node: an #IdeXmlSymbolNode
 *
 * Create a new #IdeXmlSymbolTree
 *
 * Returns: (transfer full): A newly allocated #IdeXmlSymbolTree.
 */
IdeXmlSymbolTree *
ide_xml_symbol_tree_new (IdeXmlSymbolNode *root_node)
{
  IdeXmlSymbolTree *self;

  g_return_val_if_fail (IDE_IS_XML_SYMBOL_NODE (root_node), NULL);

  self = g_object_new (IDE_TYPE_XML_SYMBOL_TREE, NULL);
  self->root_node = g_object_ref (root_node);

  return self;
}

static void
ide_xml_symbol_tree_finalize (GObject *object)
{
  IdeXmlSymbolTree *self = (IdeXmlSymbolTree *)object;

  g_clear_object (&self->root_node);

  G_OBJECT_CLASS (ide_xml_symbol_tree_parent_class)->finalize (object);
}

static void
ide_xml_symbol_tree_class_init (IdeXmlSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_xml_symbol_tree_finalize;
}

static void
ide_xml_symbol_tree_init (IdeXmlSymbolTree *self)
{
}
