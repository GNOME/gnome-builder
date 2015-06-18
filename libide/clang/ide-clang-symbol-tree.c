/* ide-clang-symbol-tree.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#include <glib/gi18n.h>

#include "ide-clang-symbol-tree.h"
#include "ide-ref-ptr.h"

struct _IdeClangSymbolTree
{
  GObject    parent_instance;

  IdeRefPtr *native;
};

static void symbol_tree_iface_init (IdeSymbolTreeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeClangSymbolTree, ide_clang_symbol_tree, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_TREE, symbol_tree_iface_init))

enum {
  PROP_0,
  PROP_NATIVE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static guint
ide_clang_symbol_tree_get_n_children (IdeSymbolTree *symbol_tree,
                                      IdeSymbolNode *parent)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), 0);
  g_return_val_if_fail (!parent || IDE_IS_SYMBOL_NODE (parent), 0);

  return 0;
}

static IdeSymbolNode *
ide_clang_symbol_tree_get_nth_child (IdeSymbolTree *symbol_tree,
                                     IdeSymbolNode *parent,
                                     guint          nth)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)symbol_tree;

  g_return_val_if_fail (IDE_IS_CLANG_SYMBOL_TREE (self), NULL);
  g_return_val_if_fail (!parent || IDE_IS_SYMBOL_NODE (parent), NULL);

  return NULL;
}

static void
ide_clang_symbol_tree_finalize (GObject *object)
{
  IdeClangSymbolTree *self = (IdeClangSymbolTree *)object;

  g_clear_pointer (&self->native, ide_ref_ptr_unref);

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
    case PROP_NATIVE:
      g_value_set_boxed (value, self->native);
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
    case PROP_NATIVE:
      self->native = g_value_dup_boxed (value);
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

  gParamSpecs [PROP_NATIVE] =
    g_param_spec_boxed ("native",
                        _("Native"),
                        _("Native"),
                        IDE_TYPE_REF_PTR,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
ide_clang_symbol_tree_init (IdeClangSymbolTree *self)
{
}

static void
symbol_tree_iface_init (IdeSymbolTreeInterface *iface)
{
  iface->get_n_children = ide_clang_symbol_tree_get_n_children;
  iface->get_nth_child = ide_clang_symbol_tree_get_nth_child;
}
