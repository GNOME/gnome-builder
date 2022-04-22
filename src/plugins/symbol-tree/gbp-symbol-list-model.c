/* gbp-symbol-list-model.c
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

#define G_LOG_DOMAIN "gbp-symbol-list-model"

#include "config.h"

#include "gbp-symbol-list-model.h"

struct _GbpSymbolListModel
{
  GObject        parent_instance;
  IdeSymbolTree *tree;
  IdeSymbolNode *parent;
};

enum {
  PROP_0,
  PROP_TREE,
  PROP_PARENT,
  N_PROPS
};

static GType
gbp_symbol_list_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_SYMBOL_NODE;
}

static guint
gbp_symbol_list_model_get_n_items (GListModel *model)
{
  GbpSymbolListModel *self = GBP_SYMBOL_LIST_MODEL (model);

  return ide_symbol_tree_get_n_children (self->tree, self->parent);
}

static gpointer
gbp_symbol_list_model_get_item (GListModel *model,
                                guint       position)
{
  GbpSymbolListModel *self = GBP_SYMBOL_LIST_MODEL (model);

  if (position >= ide_symbol_tree_get_n_children (self->tree, self->parent))
    return NULL;

  return ide_symbol_tree_get_nth_child (self->tree, self->parent, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = gbp_symbol_list_model_get_item;
  iface->get_n_items = gbp_symbol_list_model_get_n_items;
  iface->get_item_type = gbp_symbol_list_model_get_item_type;
}

G_DEFINE_TYPE_WITH_CODE (GbpSymbolListModel, gbp_symbol_list_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_symbol_list_model_dispose (GObject *object)
{
  GbpSymbolListModel *self = (GbpSymbolListModel *)object;

  g_clear_object (&self->tree);
  g_clear_object (&self->parent);

  G_OBJECT_CLASS (gbp_symbol_list_model_parent_class)->dispose (object);
}

static void
gbp_symbol_list_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpSymbolListModel *self = GBP_SYMBOL_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_TREE:
      g_value_set_object (value, self->tree);
      break;

    case PROP_PARENT:
      g_value_set_object (value, self->parent);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpSymbolListModel *self = GBP_SYMBOL_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_TREE:
      self->tree = g_value_dup_object (value);
      break;

    case PROP_PARENT:
      self->parent = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_list_model_class_init (GbpSymbolListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_symbol_list_model_dispose;
  object_class->get_property = gbp_symbol_list_model_get_property;
  object_class->set_property = gbp_symbol_list_model_set_property;

  properties [PROP_TREE] =
    g_param_spec_object ("tree",
                         "Tree",
                         "The tree of nodes",
                         IDE_TYPE_SYMBOL_TREE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARENT] =
    g_param_spec_object ("parent",
                         "Parent",
                         "The parent node",
                         IDE_TYPE_SYMBOL_NODE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_symbol_list_model_init (GbpSymbolListModel *self)
{
}

IdeSymbolTree *
gbp_symbol_list_model_get_tree (GbpSymbolListModel *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_LIST_MODEL (self), NULL);

  return self->tree;
}

GbpSymbolListModel *
gbp_symbol_list_model_new (IdeSymbolTree *tree,
                           IdeSymbolNode *parent)
{
  return g_object_new (GBP_TYPE_SYMBOL_LIST_MODEL,
                       "tree", tree,
                       "parent", parent,
                       NULL);
}
