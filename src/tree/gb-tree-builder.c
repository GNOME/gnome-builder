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

enum
{
	PROP_0,
	PROP_TREE,
	LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * gb_tree_builder_node_activated:
 * @builder: (in): A #GbTreeBuilder.
 * @node: (in): A #GbTreeNode.
 *
 * Handle @node being activated. Builders may want to open a view
 * or perform an action on such an event.
 *
 * Returns: %TRUE if the node activation was handled.
 */
gboolean
gb_tree_builder_node_activated (GbTreeBuilder *builder,
                                GbTreeNode    *node)
{
	g_return_val_if_fail (GB_IS_TREE_BUILDER(builder), FALSE);
	g_return_val_if_fail (GB_IS_TREE_NODE(node), FALSE);

	if (GB_TREE_BUILDER_GET_CLASS (builder)->node_activated)
		return GB_TREE_BUILDER_GET_CLASS (builder)->node_activated(builder, node);

	return FALSE;
}

void
gb_tree_builder_node_popup (GbTreeBuilder *builder,
                            GbTreeNode    *node,
                            GMenu         *menu)
{
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (G_IS_MENU (menu));

  if (GB_TREE_BUILDER_GET_CLASS (builder)->node_popup)
    GB_TREE_BUILDER_GET_CLASS (builder)->node_popup (builder, node, menu);
}

/**
 * gb_tree_builder_node_selected:
 * @builder: (in): A #GbTreeBuilder.
 * @node: (in): A #GbTreeNode.
 *
 * Update @node for being selected and update any actions or ui based
 * on @node being selected.
 */
void
gb_tree_builder_node_selected (GbTreeBuilder *builder,
                               GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

	if (GB_TREE_BUILDER_GET_CLASS (builder)->node_selected)
		GB_TREE_BUILDER_GET_CLASS (builder)->node_selected (builder, node);
}

/**
 * gb_tree_builder_node_unselected:
 * @builder: (in): A #GbTreeBuilder.
 * @node: (in): A #GbTreeNode.
 *
 * Update @node and any actions that may be related to @node to account
 * for it being unselected within the #GbTree.
 */
void
gb_tree_builder_node_unselected (GbTreeBuilder *builder,
                                 GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

	if (GB_TREE_BUILDER_GET_CLASS (builder)->node_selected)
		GB_TREE_BUILDER_GET_CLASS (builder)->node_unselected (builder, node);
}

/**
 * gb_tree_builder_build_node:
 * @builder: (in): A #GbTreeBuilder.
 * @node: (in): A #GbTreeNode.
 *
 * Build @node by setting any needed properties for the item or
 * updating it's appearance. Additional actions may be registered
 * based on @node's type if needed.
 */
void
gb_tree_builder_build_node (GbTreeBuilder *builder,
                            GbTreeNode    *node)
{
	g_return_if_fail (GB_IS_TREE_BUILDER (builder));
	g_return_if_fail (GB_IS_TREE_NODE (node));

	if (GB_TREE_BUILDER_GET_CLASS (builder)->build_node)
		GB_TREE_BUILDER_GET_CLASS (builder)->build_node (builder, node);
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
}

static void
gb_tree_builder_init (GbTreeBuilder *builder)
{
}
