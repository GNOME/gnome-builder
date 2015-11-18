/* ide-tree-builder.c
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

#include "ide-tree.h"
#include "ide-tree-builder.h"

typedef struct
{
	IdeTree *tree;
} IdeTreeBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTreeBuilder, ide_tree_builder, G_TYPE_INITIALLY_UNOWNED)

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

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

gboolean
_ide_tree_builder_node_activated (IdeTreeBuilder *builder,
                                 IdeTreeNode    *node)
{
  gboolean ret = FALSE;

	g_return_val_if_fail (IDE_IS_TREE_BUILDER(builder), FALSE);
	g_return_val_if_fail (IDE_IS_TREE_NODE(node), FALSE);

  g_signal_emit (builder, signals [NODE_ACTIVATED], 0, node, &ret);

	return ret;
}

void
_ide_tree_builder_node_popup (IdeTreeBuilder *builder,
                             IdeTreeNode    *node,
                             GMenu         *menu)
{
  g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (G_IS_MENU (menu));

  g_signal_emit (builder, signals [NODE_POPUP], 0, node, menu);
}

void
_ide_tree_builder_node_selected (IdeTreeBuilder *builder,
                                IdeTreeNode    *node)
{
	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (IDE_IS_TREE_NODE (node));

  g_signal_emit (builder, signals [NODE_SELECTED], 0, node);
}

void
_ide_tree_builder_node_unselected (IdeTreeBuilder *builder,
                                  IdeTreeNode    *node)
{
	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (IDE_IS_TREE_NODE (node));

  g_signal_emit (builder, signals [NODE_UNSELECTED], 0, node);
}

void
_ide_tree_builder_build_node (IdeTreeBuilder *builder,
                             IdeTreeNode    *node)
{
	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (IDE_IS_TREE_NODE (node));

  g_signal_emit (builder, signals [BUILD_NODE], 0, node);
}

void
_ide_tree_builder_added (IdeTreeBuilder *builder,
                        IdeTree        *tree)
{
	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (IDE_IS_TREE (tree));

  g_signal_emit (builder, signals [ADDED], 0, tree);
}

void
_ide_tree_builder_removed (IdeTreeBuilder *builder,
                          IdeTree        *tree)
{
	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (IDE_IS_TREE (tree));

  g_signal_emit (builder, signals [REMOVED], 0, tree);
}

void
_ide_tree_builder_set_tree (IdeTreeBuilder *builder,
                           IdeTree        *tree)
{
	IdeTreeBuilderPrivate *priv = ide_tree_builder_get_instance_private (builder);

	g_return_if_fail (IDE_IS_TREE_BUILDER (builder));
	g_return_if_fail (priv->tree == NULL);
	g_return_if_fail (IDE_IS_TREE (tree));

  if (priv->tree != tree)
    {
      if (priv->tree != NULL)
        {
          g_object_remove_weak_pointer (G_OBJECT (priv->tree), (gpointer *)&priv->tree);
          priv->tree = NULL;
        }

      if (tree != NULL)
        {
          priv->tree = tree;
          g_object_add_weak_pointer (G_OBJECT (priv->tree), (gpointer *)&priv->tree);
        }

      g_object_notify_by_pspec (G_OBJECT (builder), properties [PROP_TREE]);
    }
}

/**
 * ide_tree_builder_get_tree:
 * @builder: (in): A #IdeTreeBuilder.
 *
 * Gets the tree that owns the builder.
 *
 * Returns: (transfer none) (type IdeTree) (nullable): A #IdeTree or %NULL.
 */
IdeTree *
ide_tree_builder_get_tree (IdeTreeBuilder *builder)
{
  IdeTreeBuilderPrivate *priv = ide_tree_builder_get_instance_private (builder);

  g_return_val_if_fail (IDE_IS_TREE_BUILDER (builder), NULL);

  return priv->tree;
}

static void
ide_tree_builder_finalize (GObject *object)
{
	IdeTreeBuilder *builder = IDE_TREE_BUILDER (object);
	IdeTreeBuilderPrivate *priv = ide_tree_builder_get_instance_private (builder);

	if (priv->tree)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->tree), (gpointer *)&priv->tree);
      priv->tree = NULL;
    }

	G_OBJECT_CLASS (ide_tree_builder_parent_class)->finalize (object);
}

static void
ide_tree_builder_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	IdeTreeBuilder *builder = IDE_TREE_BUILDER (object);
  IdeTreeBuilderPrivate *priv = ide_tree_builder_get_instance_private (builder);

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
ide_tree_builder_class_init (IdeTreeBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ide_tree_builder_finalize;
	object_class->get_property = ide_tree_builder_get_property;

	properties[PROP_TREE] =
		g_param_spec_object("tree",
		                    "Tree",
		                    "The IdeTree the builder belongs to.",
		                    IDE_TYPE_TREE,
		                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TREE);

  signals [BUILD_NODE] =
    g_signal_new ("build-node",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, build_node),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TREE_NODE);

  signals [NODE_ACTIVATED] =
    g_signal_new ("node-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, node_activated),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  1,
                  IDE_TYPE_TREE_NODE);

  signals [NODE_POPUP] =
    g_signal_new ("node-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, node_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_TREE_NODE,
                  G_TYPE_MENU);

  signals [NODE_SELECTED] =
    g_signal_new ("node-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, node_selected),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TREE_NODE);

  signals [NODE_UNSELECTED] =
    g_signal_new ("node-unselected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, node_unselected),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TREE_NODE);

  signals [REMOVED] =
    g_signal_new ("removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeBuilderClass, removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TREE);
}

static void
ide_tree_builder_init (IdeTreeBuilder *builder)
{
}
