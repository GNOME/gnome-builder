/* gb-tree-builder.c
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#define G_LOG_DOMAIN "tree-builder"

#include <glib/gi18n.h>

#include "gb-tree.h"
#include "gb-tree-builder.h"

typedef struct
{
	GbTree *tree;
} GbTreeBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GbTreeBuilder, gb_tree_builder, G_TYPE_INITIALLY_UNOWNED)

enum {
	PROP_0,
	PROP_TREE,
	LAST_PROP
};

enum {
  ADDED,
  REMOVED,
  BUILD_NODE,
  NODE_ACTIVATED,
  NODE_POPUP,
  NODE_SELECTED,
  NODE_UNSELECTED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

gboolean
_gb_tree_builder_node_activated (GbTreeBuilder *builder,
                                 GbTreeNode    *node)
{
  gboolean ret = FALSE;

	g_return_val_if_fail (GB_IS_TREE_BUILDER(builder), FALSE);
	g_return_val_if_fail (GB_IS_TREE_NODE(node), FALSE);

  g_signal_emit (builder, gSignals [NODE_ACTIVATED], 0, node, &ret);

	return ret;
}

void
_gb_tree_builder_node_popup (GbTreeBuilder *builder,
                             GbTreeNode    *node,
                             GMenu         *menu)
{
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (G_IS_MENU (menu));

  g_signal_emit (builder, gSignals [NODE_POPUP], 0, node, menu);
}

void
_gb_tree_builder_node_selected (GbTreeBuilder *builder,
                                GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

  g_signal_emit (builder, gSignals [NODE_SELECTED], 0, node);
}

void
_gb_tree_builder_node_unselected (GbTreeBuilder *builder,
                                  GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

  g_signal_emit (builder, gSignals [NODE_UNSELECTED], 0, node);
}

void
_gb_tree_builder_build_node (GbTreeBuilder *builder,
                             GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

  g_signal_emit (builder, gSignals [BUILD_NODE], 0, node);
}

void
_gb_tree_builder_added (GbTreeBuilder *builder,
                        GbTree        *tree)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE (tree));

  g_signal_emit (builder, gSignals [ADDED], 0, tree);
}

void
_gb_tree_builder_removed (GbTreeBuilder *builder,
                          GbTree        *tree)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE (tree));

  g_signal_emit (builder, gSignals [REMOVED], 0, tree);
}

/**
 * gb_tree_builder_set_tree:
 * @builder: (in): A #GbTreeBuilder.
 * @tree: (in): A #GbTree.
 *
 * Sets the tree the builder is associated with.
 */
static void
gb_tree_builder_set_tree (GbTreeBuilder *builder,
                          GbTree        *tree)
{
	GbTreeBuilderPrivate *priv = gb_tree_builder_get_instance_private (builder);

	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (priv->tree == NULL);
	g_return_if_fail (GB_IS_TREE (tree));

	if (tree)
    {
      priv->tree = tree;
      g_object_add_weak_pointer (G_OBJECT (priv->tree), (gpointer *)&priv->tree);
    }
}

/**
 * gb_tree_builder_get_tree:
 * @builder: (in): A #GbTreeBuilder.
 *
 * Gets the tree that owns the builder.
 *
 * Returns: (transfer none) (type GbTree) (nullable): A #GbTree or %NULL.
 */
GtkWidget *
gb_tree_builder_get_tree (GbTreeBuilder *builder)
{
  GbTreeBuilderPrivate *priv = gb_tree_builder_get_instance_private (builder);

  g_return_val_if_fail (GB_IS_TREE_BUILDER (builder), NULL);

  return (GtkWidget *)priv->tree;
}

static void
gb_tree_builder_finalize (GObject *object)
{
	GbTreeBuilder *builder = GB_TREE_BUILDER (object);
	GbTreeBuilderPrivate *priv = gb_tree_builder_get_instance_private (builder);

	if (priv->tree)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->tree), (gpointer *)&priv->tree);
      priv->tree = NULL;
    }

	G_OBJECT_CLASS (gb_tree_builder_parent_class)->finalize (object);
}

static void
gb_tree_builder_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	GbTreeBuilder *builder = GB_TREE_BUILDER (object);
  GbTreeBuilderPrivate *priv = gb_tree_builder_get_instance_private (builder);

	switch (prop_id)
    {
    case PROP_TREE:
      g_value_set_object (value, priv->tree);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tree_builder_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	GbTreeBuilder *builder = GB_TREE_BUILDER (object);

	switch (prop_id)
    {
    case PROP_TREE:
      gb_tree_builder_set_tree (builder, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tree_builder_class_init (GbTreeBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gb_tree_builder_finalize;
	object_class->get_property = gb_tree_builder_get_property;
	object_class->set_property = gb_tree_builder_set_property;

	gParamSpecs[PROP_TREE] =
		g_param_spec_object("tree",
		                    _("Tree"),
		                    _("The GbTree the builder belongs to."),
		                    GB_TYPE_TREE,
		                    G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_TREE);

  gSignals [BUILD_NODE] =
    g_signal_new ("build-node",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, build_node),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_TREE_NODE);

  gSignals [NODE_ACTIVATED] =
    g_signal_new ("node-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, node_activated),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  1,
                  GB_TYPE_TREE_NODE);

  gSignals [NODE_POPUP] =
    g_signal_new ("node-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, node_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  GB_TYPE_TREE_NODE,
                  G_TYPE_MENU);

  gSignals [NODE_SELECTED] =
    g_signal_new ("node-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, node_selected),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_TREE_NODE);

  gSignals [NODE_UNSELECTED] =
    g_signal_new ("node-unselected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, node_unselected),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_TREE_NODE);

  gSignals [REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeBuilderClass, removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_TREE);
}

static void
gb_tree_builder_init (GbTreeBuilder *builder)
{
}
