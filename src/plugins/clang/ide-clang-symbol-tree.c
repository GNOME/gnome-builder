/* ide-clang-symbol-tree.c
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

#define G_LOG_DOMAIN "ide-clang-symbol-tree"

#include <clang-c/Index.h>
#include <glib/gi18n.h>

#include "ide-clang-symbol-node.h"
#include "ide-clang-symbol-tree.h"

struct _IdeClangSymbolTree
{
  GObject    parent_instance;
  GVariant  *tree;
  GFile     *file;
};

static void symbol_tree_iface_init (IdeSymbolTreeInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangSymbolTree, ide_clang_symbol_tree, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_clang_symbol_tree_get_file:
 * @self: an #IdeClangSymbolTree.
 *
 * Gets the #IdeClangSymbolTree:file property.
 *
 * Returns: (transfer none): a #GFile.
 */
GFile *
ide_clang_symbol_tree_get_file (IdeClangSymbolTree *self)
{
  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), NULL);

  return self->file;
}

static guint
ide_clang_symbol_tree_get_n_children (IdeSymbolTree *symbol_tree,
                                      IdeSymbolNode *parent)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;

  g_assert (IDE_IS_CLANG_SYMBOL_TREE (self));
  g_assert (!parent || IDE_IS_CLANG_SYMBOL_NODE (parent));

  if (parent == NULL)
    return self->tree ? g_variant_n_children (self->tree) : 0;

  return ide_clang_symbol_node_get_n_children (IDE_CLANG_SYMBOL_NODE (parent));
}

static IdeSymbolNode *
ide_clang_symbol_tree_get_nth_child (IdeSymbolTree *symbol_tree,
                                     IdeSymbolNode *parent,
                                     guint          nth)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;
  g_autoptr(GVariant) node = NULL;
  IdeSymbolNode *ret;

  g_assert (IDE_IS_CLANG_SYMBOL_TREE (self));
  g_assert (!parent || IDE_IS_CLANG_SYMBOL_NODE (parent));

  if (parent != NULL)
    return ide_clang_symbol_node_get_nth_child (IDE_CLANG_SYMBOL_NODE (parent), nth);

  if (self->tree == NULL)
    g_return_val_if_reached (NULL);

  if (nth >= g_variant_n_children (self->tree))
    g_return_val_if_reached (NULL);

  node = g_variant_get_child_value (self->tree, nth);
  ret = ide_clang_symbol_node_new (node);

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_NODE (ret), NULL);

  return ret;
}

static void
ide_clang_symbol_tree_finalize (GObject *object)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)object;

  g_clear_pointer (&self->tree, g_variant_unref);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_clang_symbol_tree_parent_class)->finalize (object);
}

static void
ide_clang_symbol_tree_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeClangSymbolTree *self = IDE_CLANG_SYMBOL_TREE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_clang_symbol_tree_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_symbol_tree_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeClangSymbolTree *self = IDE_CLANG_SYMBOL_TREE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_symbol_tree_class_init (IdeClangSymbolTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_symbol_tree_finalize;
  object_class->get_property = ide_clang_symbol_tree_get_property;
  object_class->set_property = ide_clang_symbol_tree_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "File",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_clang_symbol_tree_init (IdeClangSymbolTree *self)
{
}

IdeClangSymbolTree *
ide_clang_symbol_tree_new (GFile      *file,
                           GVariant   *tree)
{
  IdeClangSymbolTree *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!tree ||
                        g_variant_is_of_type (tree, G_VARIANT_TYPE ("av")) ||
                        g_variant_is_of_type (tree, G_VARIANT_TYPE ("aa{sv}")),
                        NULL);

  self = g_object_new (IDE_TYPE_CLANG_SYMBOL_TREE,
                       "file", file,
                       NULL);

  if (tree != NULL)
    self->tree = g_variant_ref_sink (tree);

  return self;
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_clang_symbol_tree_get_n_children;
  iface->get_nth_child = ide_clang_symbol_tree_get_nth_child;
}
