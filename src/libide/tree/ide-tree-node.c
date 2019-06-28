/* ide-tree-node.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tree-node"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-tree-model.h"
#include "ide-tree-node.h"
#include "ide-tree-private.h"

/**
 * SECTION:ide-tree-node
 * @title: IdeTreeNode
 * @short_description: a node within the tree
 *
 * The #IdeTreeNode class is used to represent an item that should
 * be displayed in the tree of the Ide application. The
 * #IdeTreeAddin plugins create and maintain these nodes during the
 * lifetime of the program.
 *
 * Plugins that want to add items to the tree should implement the
 * #IdeTreeAddin interface and register it during plugin
 * initialization.
 *
 * Since: 3.32
 */

struct _IdeTreeNode
{
  GObject parent_instance;

  /* A pointer to the model, which is only set on the root node. */
  IdeTreeModel *model;

  /*
   * The following are fields containing the values for various properties
   * on the tree node. Usually, icon, display_name, and item will be set
   * on all nodes.
   */
  GIcon   *icon;
  GIcon   *expanded_icon;
  gchar   *display_name;
  GObject *item;
  gchar   *tag;
  GList   *emblems;

  /*
   * The following items are used to maintain a tree structure of
   * nodes for which we can use O(1) operations. The link is inserted
   * into the parents children queue. The parent pointer is unowned,
   * and set by the parent (cleared upon removal).
   *
   * This also allows maintaining the tree structure with zero additional
   * allocations beyond the nodes themselves.
   */
  IdeTreeNode *parent;
  GQueue       children;
  GList        link;

  /* Foreground and Background colors */
  GdkRGBA      background;
  GdkRGBA      foreground;

  /* Flags for state cell renderer */
  IdeTreeNodeFlags flags;

  /* When did we start loading? This is used to avoid drawing "Loading..."
   * when the tree loads really quickly. Otherwise, we risk looking janky
   * when the loads are quite fast.
   */
  gint64 started_loading_at;

  /* If we're currently loading */
  guint is_loading : 1;

  /* If the node is a header (bold, etc) */
  guint is_header : 1;

  /* If this is a synthesized empty node */
  guint is_empty : 1;

  /* If there are errors associated with the node's item */
  guint has_error : 1;

  /* If the node maybe has children */
  guint children_possible : 1;

  /* If this node needs to have the children built */
  guint needs_build_children : 1;

  /* If true, we remove all children on collapse */
  guint reset_on_collapse : 1;

  /* If pango markup should be used */
  guint use_markup : 1;

  /* If true, we use ide_clear_and_destroy_object() */
  guint destroy_item : 1;

  /* If colors are set */
  guint background_set : 1;
  guint foreground_set : 1;
};

G_DEFINE_TYPE (IdeTreeNode, ide_tree_node, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CHILDREN_POSSIBLE,
  PROP_DESTROY_ITEM,
  PROP_DISPLAY_NAME,
  PROP_EXPANDED_ICON,
  PROP_EXPANDED_ICON_NAME,
  PROP_HAS_ERROR,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_IS_HEADER,
  PROP_ITEM,
  PROP_RESET_ON_COLLAPSE,
  PROP_TAG,
  PROP_USE_MARKUP,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static IdeTreeModel *
ide_tree_node_get_model (IdeTreeNode *self)
{
  return ide_tree_node_get_root (self)->model;
}

/**
 * ide_tree_node_new:
 *
 * Create a new #IdeTreeNode.
 *
 * Returns: (transfer full): a newly created #IdeTreeNode
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_new (void)
{
  return g_object_new (IDE_TYPE_TREE_NODE, NULL);
}

static void
ide_tree_node_emit_changed (IdeTreeNode *self)
{
  g_autoptr(GtkTreePath) path = NULL;
  IdeTreeModel *model;
  GtkTreeIter iter = { .user_data = self };

  g_assert (IDE_IS_TREE_NODE (self));

  if (!(model = ide_tree_node_get_model (self)))
    return;

  if ((path = ide_tree_model_get_path_for_node (model, self)))
    gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
}

static void
ide_tree_node_remove_with_dispose (IdeTreeNode *self,
                                   IdeTreeNode *child)
{
  g_object_ref (child);
  ide_tree_node_remove (self, child);
  g_object_run_dispose (G_OBJECT (child));
  g_object_unref (child);
}

static void
ide_tree_node_dispose (GObject *object)
{
  IdeTreeNode *self = (IdeTreeNode *)object;

  while (self->children.length > 0)
    ide_tree_node_remove_with_dispose (self, g_queue_peek_nth (&self->children, 0));

  if (self->destroy_item && IDE_IS_OBJECT (self->item))
    ide_clear_and_destroy_object (&self->item);
  else
    g_clear_object (&self->item);

  g_list_free_full (self->emblems, g_object_unref);
  self->emblems = NULL;

  g_clear_object (&self->icon);
  g_clear_object (&self->expanded_icon);
  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->tag, g_free);

  G_OBJECT_CLASS (ide_tree_node_parent_class)->dispose (object);
}

static void
ide_tree_node_finalize (GObject *object)
{
  IdeTreeNode *self = (IdeTreeNode *)object;

  g_clear_weak_pointer (&self->model);

  g_assert (self->children.head == NULL);
  g_assert (self->children.tail == NULL);
  g_assert (self->children.length == 0);

  if (self->destroy_item && IDE_IS_OBJECT (self->item))
    ide_clear_and_destroy_object (&self->item);
  else
    g_clear_object (&self->item);

  g_clear_object (&self->icon);
  g_clear_object (&self->expanded_icon);
  g_clear_pointer (&self->display_name, g_free);
  g_clear_pointer (&self->tag, g_free);

  G_OBJECT_CLASS (ide_tree_node_parent_class)->finalize (object);
}

static void
ide_tree_node_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeTreeNode *self = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      g_value_set_boolean (value, ide_tree_node_get_children_possible (self));
      break;

    case PROP_DESTROY_ITEM:
      g_value_set_boolean (value, self->destroy_item);
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_tree_node_get_display_name (self));
      break;

    case PROP_HAS_ERROR:
      g_value_set_boolean (value, ide_tree_node_get_has_error (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, ide_tree_node_get_icon (self));
      break;

    case PROP_IS_HEADER:
      g_value_set_boolean (value, ide_tree_node_get_is_header (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, ide_tree_node_get_item (self));
      break;

    case PROP_RESET_ON_COLLAPSE:
      g_value_set_boolean (value, ide_tree_node_get_reset_on_collapse (self));
      break;

    case PROP_TAG:
      g_value_set_string (value, ide_tree_node_get_tag (self));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, ide_tree_node_get_use_markup (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_node_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeTreeNode *self = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      ide_tree_node_set_children_possible (self, g_value_get_boolean (value));
      break;

    case PROP_DESTROY_ITEM:
      self->destroy_item = g_value_get_boolean (value);
      break;

    case PROP_DISPLAY_NAME:
      ide_tree_node_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_EXPANDED_ICON:
      ide_tree_node_set_expanded_icon (self, g_value_get_object (value));
      break;

    case PROP_EXPANDED_ICON_NAME:
      ide_tree_node_set_expanded_icon_name (self, g_value_get_string (value));
      break;

    case PROP_HAS_ERROR:
      ide_tree_node_set_has_error (self, g_value_get_boolean (value));
      break;

    case PROP_ICON:
      ide_tree_node_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      ide_tree_node_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_IS_HEADER:
      ide_tree_node_set_is_header (self, g_value_get_boolean (value));
      break;

    case PROP_ITEM:
      ide_tree_node_set_item (self, g_value_get_object (value));
      break;

    case PROP_RESET_ON_COLLAPSE:
      ide_tree_node_set_reset_on_collapse (self, g_value_get_boolean (value));
      break;

    case PROP_TAG:
      ide_tree_node_set_tag (self, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      ide_tree_node_set_use_markup (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_node_class_init (IdeTreeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tree_node_dispose;
  object_class->finalize = ide_tree_node_finalize;
  object_class->get_property = ide_tree_node_get_property;
  object_class->set_property = ide_tree_node_set_property;

  /**
   * IdeTreeNode:children-possible:
   *
   * The "children-possible" property denotes if the node may have children
   * even if it doesn't have children yet. This is useful for delayed loading
   * of children nodes.
   *
   * Since: 3.32
   */
  properties [PROP_CHILDREN_POSSIBLE] =
    g_param_spec_boolean ("children-possible",
                          "Children Possible",
                          "If children are possible for the node",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:destroy-item:
   *
   * If %TRUE and #IdeTreeNode:item is an #IdeObject, it will be destroyed
   * when the node is destroyed.
   *
   * Since: 3.32
   */
  properties [PROP_DESTROY_ITEM] =
    g_param_spec_boolean ("destroy-item",
                          "Destroy Item",
                          "If the item should be destroyed with the node.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:display-name:
   *
   * The "display-name" property is the name for the node as it should be
   * displayed in the tree.
   *
   * Since: 3.32
   */
  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display name for the node in the tree",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:expanded-icon:
   *
   * The "expanded-icon" property is the icon that should be displayed to the
   * user in the tree for this node.
   *
   * Since: 3.32
   */
  properties [PROP_EXPANDED_ICON] =
    g_param_spec_object ("expanded-icon",
                         "Expanded Icon",
                         "The expanded icon to display in the tree",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:expanded-icon-name:
   *
   * The "expanded-icon-name" is a convenience property to set the
   * #IdeTreeNode:expanded-icon property using an icon-name.
   *
   * Since: 3.32
   */
  properties [PROP_EXPANDED_ICON_NAME] =
    g_param_spec_string ("expanded-icon-name",
                         "Expanded Icon Name",
                         "The expanded icon-name for the GIcon",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:has-error:
   *
   * The "has-error" property is true if the node should be rendered with
   * an error styling. This is useful when errors are known by the diagnostics
   * manager for a given file or folder.
   *
   * Since: 3.32
   */
  properties [PROP_HAS_ERROR] =
    g_param_spec_boolean ("has-error",
                          "Has Error",
                          "If the node has an error associated with it's item",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:icon:
   *
   * The "icon" property is the icon that should be displayed to the
   * user in the tree for this node.
   *
   * Since: 3.32
   */
  properties [PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "The icon to display in the tree",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:icon-name:
   *
   * The "icon-name" is a convenience property to set the #IdeTreeNode:icon
   * property using an icon-name.
   *
   * Since: 3.32
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon-name for the GIcon",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:is-header:
   *
   * The "is-header" property denotes the node should be styled as a group
   * header.
   *
   * Since: 3.32
   */
  properties [PROP_IS_HEADER] =
    g_param_spec_boolean ("is-header",
                          "Is Header",
                          "If the node is a header",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:item:
   *
   * The "item" property is an optional #GObject that can be used to
   * store information about the node, which is sometimes useful when
   * creating #IdeTreeAddin plugins.
   *
   * Since: 3.32
   */
  properties [PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "Item",
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:reset-on-collapse:
   *
   * The "reset-on-collapse" denotes that children should be removed when
   * the node is collapsed.
   *
   * Since: 3.32
   */
  properties [PROP_RESET_ON_COLLAPSE] =
    g_param_spec_boolean ("reset-on-collapse",
                          "Reset on Collapse",
                          "If the children are removed when the node is collapsed",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:tag:
   *
   * The "tag" property can be used to denote the type of node when you do not have an
   * object to assign to #IdeTreeNode:item.
   *
   * See ide_tree_node_is_tag() to match a tag when building.
   *
   * Since: 3.32
   */
  properties [PROP_TAG] =
    g_param_spec_string ("tag",
                         "Tag",
                         "The tag for the node if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeNode:use-markup:
   *
   * If #TRUE, the "use-markup" property denotes that #IdeTreeNode:display-name
   * contains pango markup.
   *
   * Since: 3.32
   */
  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup",
                          "Use Markup",
                          "If pango markup should be used",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tree_node_init (IdeTreeNode *self)
{
  self->reset_on_collapse = TRUE;
  self->link.data = self;
}

/**
 * ide_tree_node_get_display_name:
 * @self: a #IdeTreeNode
 *
 * Gets the #IdeTreeNode:display-name property.
 *
 * Returns: (nullable): a string containing the display name
 *
 * Since: 3.32
 */
const gchar *
ide_tree_node_get_display_name (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->display_name;
}

/**
 * ide_tree_node_set_display_name:
 *
 * Sets the #IdeTreeNode:display-name property, which is the text to
 * use when displaying the item in the tree.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_display_name (IdeTreeNode *self,
                                const gchar *display_name)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (g_strcmp0 (display_name, self->display_name) != 0)
    {
      g_free (self->display_name);
      self->display_name = g_strdup (display_name);
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

/**
 * ide_tree_node_get_icon:
 * @self: a #IdeTree
 *
 * Gets the icon associated with the tree node.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 *
 * Since: 3.32
 */
GIcon *
ide_tree_node_get_icon (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->icon;
}

/**
 * ide_tree_node_set_icon:
 * @self: a @IdeTreeNode
 * @icon: (nullable): a #GIcon or %NULL
 *
 * Sets the icon for the tree node.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_icon (IdeTreeNode *self,
                        GIcon       *icon)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (g_set_object (&self->icon, icon))
    {
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON]);
    }
}

/**
 * ide_tree_node_get_expanded_icon:
 * @self: a #IdeTree
 *
 * Gets the expanded icon associated with the tree node.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 *
 * Since: 3.32
 */
GIcon *
ide_tree_node_get_expanded_icon (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->expanded_icon ? self->expanded_icon : self->icon;
}

/**
 * ide_tree_node_set_expanded_icon:
 * @self: a @IdeTreeNode
 * @expanded_icon: (nullable): a #GIcon or %NULL
 *
 * Sets the expanded icon for the tree node.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_expanded_icon (IdeTreeNode *self,
                                 GIcon       *expanded_icon)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (g_set_object (&self->expanded_icon, expanded_icon))
    {
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXPANDED_ICON]);
    }
}

/**
 * ide_tree_node_get_item:
 * @self: a #IdeTreeNode
 *
 * Gets the item that has been associated with the node.
 *
 * Returns: (transfer none) (type GObject.Object) (nullable): a #GObject
 *   if the item has been previously set.
 *
 * Since: 3.32
 */
gpointer
ide_tree_node_get_item (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);
  g_return_val_if_fail (!self->item || G_IS_OBJECT (self->item), NULL);

  return self->item;
}

void
ide_tree_node_set_item (IdeTreeNode *self,
                        gpointer     item)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    {
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);
    }
}

static IdeTreeNodeVisit
ide_tree_node_row_inserted_traverse_cb (IdeTreeNode *node,
                                        gpointer     user_data)
{
  IdeTreeModel *model = user_data;
  g_autoptr(GtkTreePath) path = NULL;
  GtkTreeIter iter = { .user_data = node };

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (IDE_IS_TREE_MODEL (model));

  /* Ignore the root node, nothing to do with that */
  if (ide_tree_node_is_root (node))
    return IDE_TREE_NODE_VISIT_CHILDREN;

  /* It would be faster to create our paths as we traverse the tree,
   * but that complicates the traversal. Generally this path should get
   * hit very little (as usually it's only a single "child node").
   */
  if ((path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter)))
    {
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);

      if (ide_tree_node_is_first (node))
        {
          IdeTreeNode *parent = ide_tree_node_get_parent (node);

          if (!ide_tree_node_is_root (parent))
            {
              iter.user_data = parent;
              gtk_tree_path_up (path);
              gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
            }
        }
    }

  return IDE_TREE_NODE_VISIT_CHILDREN;
}

static void
ide_tree_node_row_inserted (IdeTreeNode *self,
                            IdeTreeNode *child)
{
  g_autoptr(GtkTreePath) path = NULL;
  IdeTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_NODE (self));
  g_assert (IDE_IS_TREE_NODE (child));

  if (!(model = ide_tree_node_get_model (self)) ||
      !ide_tree_model_get_iter_for_node (model, &iter, child) ||
      !(path = ide_tree_model_get_path_for_node (model, child)))
    return;

  ide_tree_node_traverse (child,
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          -1,
                          ide_tree_node_row_inserted_traverse_cb,
                          model);
}

void
_ide_tree_node_set_model (IdeTreeNode  *self,
                          IdeTreeModel *model)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (!model || IDE_IS_TREE_MODEL (model));

  if (g_set_weak_pointer (&self->model, model))
    {
      if (self->model != NULL)
        ide_tree_node_row_inserted (self, self);
    }
}

/**
 * ide_tree_node_prepend:
 * @self: a #IdeTreeNode
 * @child: a #IdeTreeNode
 *
 * Prepends @child as a child of @self at the 0 index.
 *
 * This operation is O(1).
 *
 * Since: 3.32
 */
void
ide_tree_node_prepend (IdeTreeNode *self,
                       IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == NULL);

  child->parent = self;
  g_object_ref (child);
  g_queue_push_head_link (&self->children, &child->link);

  ide_tree_node_row_inserted (self, child);
}

/**
 * ide_tree_node_append:
 * @self: a #IdeTreeNode
 * @child: a #IdeTreeNode
 *
 * Appends @child as a child of @self at the last position.
 *
 * This operation is O(1).
 *
 * Since: 3.32
 */
void
ide_tree_node_append (IdeTreeNode *self,
                      IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == NULL);

  child->parent = self;
  g_object_ref (child);
  g_queue_push_tail_link (&self->children, &child->link);

  ide_tree_node_row_inserted (self, child);
}

/**
 * ide_tree_node_insert_sorted:
 * @self: an #IdeTreeNode
 * @child: an #IdeTreeNode
 * @cmpfn: (scope call): an #IdeTreeNodeCompare
 *
 * Insert @child as a child of @self at the sorted position determined by @cmpfn
 *
 * This operation is O(n).
 *
 * Since: 3.32
 */
void
ide_tree_node_insert_sorted (IdeTreeNode        *self,
                             IdeTreeNode        *child,
                             IdeTreeNodeCompare  cmpfn)
{
  GList *link;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == NULL);

  link = g_queue_find_custom (&self->children, child, (GCompareFunc)cmpfn);

  if (link != NULL)
    ide_tree_node_insert_before (IDE_TREE_NODE (link->data), child);
  else
    ide_tree_node_append (self, child);
}

/**
 * ide_tree_node_insert_before:
 * @self: a #IdeTreeNode
 * @child: a #IdeTreeNode
 *
 * Inserts @child directly before @self by adding it to the parent of @self.
 *
 * This operation is O(1).
 *
 * Since: 3.32
 */
void
ide_tree_node_insert_before (IdeTreeNode *self,
                             IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (self->parent != NULL);
  g_return_if_fail (child->parent == NULL);

  child->parent = self->parent;
  g_object_ref (child);
  _g_queue_insert_before_link (&self->parent->children, &self->link, &child->link);

  ide_tree_node_row_inserted (self, child);
}

/**
 * ide_tree_node_insert_after:
 * @self: a #IdeTreeNode
 * @child: a #IdeTreeNode
 *
 * Inserts @child directly after @self by adding it to the parent of @self.
 *
 * This operation is O(1).
 *
 * Since: 3.32
 */
void
ide_tree_node_insert_after (IdeTreeNode *self,
                            IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (self->parent != NULL);
  g_return_if_fail (child->parent == NULL);

  child->parent = self->parent;
  g_object_ref (child);
  _g_queue_insert_after_link (&self->parent->children, &self->link, &child->link);

  ide_tree_node_row_inserted (self, child);
}

/**
 * ide_tree_node_remove:
 * @self: a #IdeTreeNode
 * @child: a #IdeTreeNode
 *
 * Removes the child node @child from @self. @self must be the parent of @child.
 *
 * This function is O(1).
 *
 * Since: 3.32
 */
void
ide_tree_node_remove (IdeTreeNode *self,
                      IdeTreeNode *child)
{
  g_autoptr(GtkTreePath) path = NULL;
  IdeTreeModel *model;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == self);

  if ((model = ide_tree_node_get_model (self)))
    path = ide_tree_model_get_path_for_node (model, child);

  child->parent = NULL;
  g_queue_unlink (&self->children, &child->link);

  if (path != NULL)
    gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

  g_object_unref (child);
}

/**
 * ide_tree_node_get_parent:
 * @self: a #IdeTreeNode
 *
 * Gets the parent node of @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_get_parent (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->parent;
}

/**
 * ide_tree_node_get_root:
 * @self: a #IdeTreeNode
 *
 * Gets the root #IdeTreeNode by following the #IdeTreeNode:parent
 * properties of each node.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_get_root (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  while (self->parent != NULL)
    self = self->parent;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self;
}

/**
 * ide_tree_node_holds:
 * @self: a #IdeTreeNode
 * @type: a #GType
 *
 * Checks to see if the #IdeTreeNode:item property matches @type
 * or is a subclass of @type.
 *
 * Returns: %TRUE if @self holds a @type item
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_holds (IdeTreeNode *self,
                     GType        type)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return G_TYPE_CHECK_INSTANCE_TYPE (self->item, type);
}

/**
 * ide_tree_node_get_index:
 * @self: a #IdeTreeNode
 *
 * Gets the position of the @self.
 *
 * Returns: the offset of @self with it's siblings.
 *
 * Since: 3.32
 */
guint
ide_tree_node_get_index (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), 0);

  if (self->parent != NULL)
    return g_list_position (self->parent->children.head, &self->link);

  return 0;
}

/**
 * ide_tree_node_get_nth_child:
 * @self: a #IdeTreeNode
 * @index_: the index of the child
 *
 * Gets the @nth child of the tree node or %NULL if it does not exist.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_get_nth_child (IdeTreeNode *self,
                             guint        index_)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return g_queue_peek_nth (&self->children, index_);
}

/**
 * ide_tree_node_get_next:
 * @self: a #IdeTreeNode
 *
 * Gets the next sibling after @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_get_next (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  if (self->link.next)
    return self->link.next->data;

  return NULL;
}

/**
 * ide_tree_node_get_previous:
 * @self: a #IdeTreeNode
 *
 * Gets the previous sibling before @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_node_get_previous (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  if (self->link.prev)
    return self->link.prev->data;

  return NULL;
}

/**
 * ide_tree_node_get_children_possible:
 * @self: a #IdeTreeNode
 *
 * Checks if the node can have children, and if so, returns %TRUE.
 * It may not actually have children yet.
 *
 * Returns: %TRUE if the children may have children
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_get_children_possible (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->children_possible;
}

/**
 * ide_tree_node_set_children_possible:
 * @self: a #IdeTreeNode
 * @children_possible: if children are possible
 *
 * Sets if the children are possible for the node.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_children_possible (IdeTreeNode *self,
                                     gboolean     children_possible)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  children_possible = !!children_possible;

  if (children_possible != self->children_possible)
    {
      self->children_possible = children_possible;
      self->needs_build_children = children_possible;

      if (self->children_possible && self->children.length == 0)
        {
          g_autoptr(IdeTreeNode) child = NULL;

          child = g_object_new (IDE_TYPE_TREE_NODE,
                                "display-name", _("(Empty)"),
                                NULL);
          child->is_empty = TRUE;
          ide_tree_node_append (self, child);

          g_assert (ide_tree_node_has_child (self) == children_possible);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHILDREN_POSSIBLE]);
    }
}

/**
 * ide_tree_node_has_child:
 * @self: a #IdeTreeNode
 *
 * Checks if @self has any children.
 *
 * Returns: %TRUE if @self has one or more children.
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_has_child (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->children.length > 0;
}

/**
 * ide_tree_node_get_n_children:
 * @self: a #IdeTreeNode
 *
 * Gets the number of children that @self contains.
 *
 * Returns: the number of children
 *
 * Since: 3.32
 */
guint
ide_tree_node_get_n_children (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), 0);

  return self->children.length;
}

/**
 * ide_tree_node_get_is_header:
 * @self: a #IdeTreeNode
 *
 * Gets the #IdeTreeNode:is-header property.
 *
 * If this is %TRUE, then the node will be rendered with alternate
 * styling for group headers.
 *
 * Returns: %TRUE if @self is a header.
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_get_is_header (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->is_header;
}

/**
 * ide_tree_node_set_is_header:
 * @self: a #IdeTreeNode
 *
 * Sets the #IdeTreeNode:is-header property.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_is_header (IdeTreeNode *self,
                             gboolean     is_header)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  is_header = !!is_header;

  if (self->is_header != is_header)
    {
      self->is_header = is_header;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_HEADER]);
    }
}

typedef struct
{
  GTraverseType       type;
  GTraverseFlags      flags;
  gint                depth;
  IdeTreeTraverseFunc callback;
  gpointer            user_data;
} IdeTreeTraversal;

static inline gboolean
can_callback_node (IdeTreeNode    *node,
                   GTraverseFlags  flags)
{
  return ((flags & G_TRAVERSE_LEAVES) && node->children.length == 0) ||
         ((flags & G_TRAVERSE_NON_LEAVES) && node->children.length > 0);
}

static gboolean
do_traversal (IdeTreeNode      *node,
              IdeTreeTraversal *traversal)
{
  const GList *iter;
  IdeTreeNodeVisit ret = IDE_TREE_NODE_VISIT_BREAK;

  if (traversal->depth < 0)
    return IDE_TREE_NODE_VISIT_CONTINUE;

  traversal->depth--;

  if (traversal->type == G_PRE_ORDER && can_callback_node (node, traversal->flags))
    {
      ret = traversal->callback (node, traversal->user_data);

      if (!ide_tree_node_is_root (node) &&
          (ret == IDE_TREE_NODE_VISIT_CONTINUE || ret == IDE_TREE_NODE_VISIT_BREAK))
        goto finish;
    }

  iter = node->children.head;

  while (iter != NULL)
    {
      IdeTreeNode *child = iter->data;

      iter = iter->next;

      ret = do_traversal (child, traversal);

      if (ret == IDE_TREE_NODE_VISIT_BREAK)
        goto finish;
    }

  if (traversal->type == G_POST_ORDER && can_callback_node (node, traversal->flags))
    ret = traversal->callback (node, traversal->user_data);

finish:
  traversal->depth++;

  return ret;
}

/**
 * ide_tree_node_traverse:
 * @self: a #IdeTreeNode
 * @traverse_type: the type of traversal, pre and post supported
 * @traverse_flags: the flags for what nodes to match
 * @max_depth: the max depth for the traversal or -1 for all
 * @traverse_func: (scope call): the callback for each matching node
 * @user_data: user data for @traverse_func
 *
 * Calls @traverse_func for each node that matches the requested
 * type, flags, and depth.
 *
 * Traversal is stopped if @traverse_func returns %TRUE.
 *
 * Since: 3.32
 */
void
ide_tree_node_traverse (IdeTreeNode         *self,
                        GTraverseType        traverse_type,
                        GTraverseFlags       traverse_flags,
                        gint                 max_depth,
                        IdeTreeTraverseFunc  traverse_func,
                        gpointer             user_data)
{
  IdeTreeTraversal traverse;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (traverse_type == G_PRE_ORDER ||
                    traverse_type == G_POST_ORDER);
  g_return_if_fail (traverse_func != NULL);

  traverse.type = traverse_type;
  traverse.flags = traverse_flags;
  traverse.depth = max_depth < 0 ? G_MAXINT : max_depth;
  traverse.callback = traverse_func;
  traverse.user_data = user_data;

  do_traversal (self, &traverse);
}

/**
 * ide_tree_node_is_empty:
 * @self: a #IdeTreeNode
 *
 * This function checks if @self is a synthesized "empty" node.
 *
 * Empty nodes are added to #IdeTreeNode that may have children in the
 * future, but are currently empty. It allows the tree to display the
 * "(Empty)" contents and show a proper expander arrow.
 *
 * Returns: %TRUE if @self is a synthesized empty node.
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_is_empty (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->is_empty;
}

gboolean
_ide_tree_node_get_needs_build_children (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->needs_build_children;
}

void
_ide_tree_node_set_needs_build_children (IdeTreeNode *self,
                                         gboolean     needs_build_children)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  self->needs_build_children = !!needs_build_children;
}

/**
 * ide_tree_node_set_icon_name:
 * @self: a #IdeTreeNode
 * @icon_name: (nullable): the name of the icon, or %NULL
 *
 * Sets the #IdeTreeNode:icon property using an icon-name.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_icon_name (IdeTreeNode *self,
                             const gchar *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);
  ide_tree_node_set_icon (self, icon);
}

/**
 * ide_tree_node_set_expanded_icon_name:
 * @self: a #IdeTreeNode
 * @expanded_icon_name: (nullable): the name of the icon, or %NULL
 *
 * Sets the #IdeTreeNode:icon property using an icon-name.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_expanded_icon_name (IdeTreeNode *self,
                                      const gchar *expanded_icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (expanded_icon_name != NULL)
    icon = g_themed_icon_new (expanded_icon_name);
  ide_tree_node_set_expanded_icon (self, icon);
}

/**
 * ide_tree_node_is_root:
 * @self: a #IdeTreeNode
 *
 * Checks if @self is the root node, meaning it has no parent.
 *
 * Returns: %TRUE if @self has no parent.
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_is_root (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->parent == NULL;
}

/**
 * ide_tree_node_is_first:
 * @self: a #IdeTreeNode
 *
 * Checks if @self is the first sibling.
 *
 * Returns: %TRUE if @self is the first sibling
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_is_first (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->link.prev == NULL;
}

/**
 * ide_tree_node_is_last:
 * @self: a #IdeTreeNode
 *
 * Checks if @self is the last sibling.
 *
 * Returns: %TRUE if @self is the last sibling
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_is_last (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->link.next == NULL;
}

static void
ide_tree_node_dump_internal (IdeTreeNode *self,
                             gint         depth)
{
  g_autofree gchar *space = g_strnfill (depth * 2, ' ');

  g_print ("%s%s\n", space, ide_tree_node_get_display_name (self));

  g_assert (self->children.length == 0 || self->children.head);
  g_assert (self->children.length == 0 || self->children.tail);
  g_assert (self->children.length > 0 || !self->children.head);
  g_assert (self->children.length > 0 || !self->children.tail);

  for (const GList *iter = self->children.head; iter; iter = iter->next)
    ide_tree_node_dump_internal (iter->data, depth + 1);
}

void
_ide_tree_node_dump (IdeTreeNode *self)
{
  ide_tree_node_dump_internal (self, 0);
}

gboolean
_ide_tree_node_get_loading (IdeTreeNode *self,
                            gint64      *started_loading_at)
{
  g_assert (IDE_IS_TREE_NODE (self));
  g_assert (started_loading_at != NULL);

  *started_loading_at = self->started_loading_at;

  return self->is_loading;
}

void
_ide_tree_node_set_loading (IdeTreeNode *self,
                            gboolean     loading)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  self->is_loading = !!loading;

  if (self->is_loading)
    self->started_loading_at = g_get_monotonic_time ();

  for (const GList *iter = self->children.head; iter; iter = iter->next)
    {
      IdeTreeNode *child = iter->data;

      if (child->is_empty)
        {
          if (loading)
            ide_tree_node_set_display_name (child, _("Loading…"));
          else
            ide_tree_node_set_display_name (child, _("(Empty)"));

          if (self->children.length > 1)
            ide_tree_node_remove (self, child);

          break;
        }
    }
}

void
_ide_tree_node_remove_all (IdeTreeNode *self)
{
  const GList *iter;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  iter = self->children.head;

  while (iter != NULL)
    {
      IdeTreeNode *child = iter->data;
      iter = iter->next;
      ide_tree_node_remove (self, child);
    }

  if (ide_tree_node_get_children_possible (self))
    {
      g_autoptr(IdeTreeNode) child = g_object_new (IDE_TYPE_TREE_NODE,
                                                   "display-name", _("(Empty)"),
                                                   NULL);
      child->is_empty = TRUE;
      ide_tree_node_append (self, child);
      _ide_tree_node_set_needs_build_children (self, TRUE);
    }
}

/**
 * ide_tree_node_get_reset_on_collapse:
 * @self: a #IdeTreeNode
 *
 * Checks if the node should have all children removed when collapsed.
 *
 * Returns: %TRUE if children are removed on collapse
 *
 * Since: 3.32
 */
gboolean
ide_tree_node_get_reset_on_collapse (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->reset_on_collapse;
}

/**
 * ide_tree_node_set_reset_on_collapse:
 * @self: a #IdeTreeNode
 * @reset_on_collapse: if the children should be removed on collapse
 *
 * If %TRUE, then children will be removed when the row is collapsed.
 *
 * Since: 3.32
 */
void
ide_tree_node_set_reset_on_collapse (IdeTreeNode *self,
                                     gboolean     reset_on_collapse)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  reset_on_collapse = !!reset_on_collapse;

  if (reset_on_collapse != self->reset_on_collapse)
    {
      self->reset_on_collapse = reset_on_collapse;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RESET_ON_COLLAPSE]);
    }
}

/**
 * ide_tree_node_get_path:
 * @self: a #IdeTreeNode
 *
 * Gets the path for the tree node.
 *
 * Returns: (transfer full) (nullable): a path or %NULL
 *
 * Since: 3.32
 */
GtkTreePath *
ide_tree_node_get_path (IdeTreeNode *self)
{
  IdeTreeModel *model;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  if ((model = ide_tree_node_get_model (self)))
    return ide_tree_model_get_path_for_node (model, self);

  return NULL;
}

static void
ide_tree_node_get_area (IdeTreeNode  *node,
                        IdeTree      *tree,
                        GdkRectangle *area)
{
  GtkTreeViewColumn *column;
  g_autoptr(GtkTreePath) path = NULL;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (IDE_IS_TREE (tree));
  g_assert (area != NULL);

  path = ide_tree_node_get_path (node);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), 0);
  gtk_tree_view_get_cell_area (GTK_TREE_VIEW (tree), path, column, area);
}

typedef struct
{
  IdeTreeNode *self;
  IdeTree     *tree;
  GtkPopover  *popover;
} PopupRequest;

static gboolean
ide_tree_node_show_popover_timeout_cb (gpointer data)
{
  PopupRequest *popreq = data;
  GdkRectangle rect;
  GtkAllocation alloc;

  g_assert (popreq);
  g_assert (IDE_IS_TREE_NODE (popreq->self));
  g_assert (GTK_IS_POPOVER (popreq->popover));

  ide_tree_node_get_area (popreq->self, popreq->tree, &rect);
  gtk_widget_get_allocation (GTK_WIDGET (popreq->tree), &alloc);

  if ((rect.x + rect.width) > (alloc.x + alloc.width))
    rect.width = (alloc.x + alloc.width) - rect.x;

  /* FIXME: Wouldn't this be better placed in a theme? */
  switch (gtk_popover_get_position (popreq->popover))
    {
    case GTK_POS_BOTTOM:
    case GTK_POS_TOP:
      rect.y += 3;
      rect.height -= 6;
      break;
    case GTK_POS_RIGHT:
    case GTK_POS_LEFT:
      rect.x += 3;
      rect.width -= 6;
      break;

    default:
      break;
    }

  gtk_popover_set_relative_to (popreq->popover, GTK_WIDGET (popreq->tree));
  gtk_popover_set_pointing_to (popreq->popover, &rect);
  gtk_popover_popup (popreq->popover);

  g_clear_object (&popreq->self);
  g_clear_object (&popreq->popover);
  g_slice_free (PopupRequest, popreq);

  return G_SOURCE_REMOVE;
}

void
_ide_tree_node_show_popover (IdeTreeNode *self,
                             IdeTree     *tree,
                             GtkPopover  *popover)
{
  GdkRectangle cell_area;
  GdkRectangle visible_rect;
  PopupRequest *popreq;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_tree_view_get_visible_rect (GTK_TREE_VIEW (tree), &visible_rect);
  ide_tree_node_get_area (self, tree, &cell_area);
  gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW (tree),
                                                   cell_area.x,
                                                   cell_area.y,
                                                   &cell_area.x,
                                                   &cell_area.y);

  popreq = g_slice_new0 (PopupRequest);
  popreq->self = g_object_ref (self);
  popreq->tree = g_object_ref (tree);
  popreq->popover = g_object_ref (popover);

  /*
   * If the node is not on screen, we need to animate until we get there.
   */
  if ((cell_area.y < visible_rect.y) ||
      ((cell_area.y + cell_area.height) >
       (visible_rect.y + visible_rect.height)))
    {
      GtkTreePath *path;

      path = ide_tree_node_get_path (self);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree), path, NULL, FALSE, 0, 0);
      g_clear_pointer (&path, gtk_tree_path_free);

      /*
       * FIXME: Time period comes from gtk animation duration.
       *        Not curently available in pubic API.
       *        We need to be greater than the max timeout it
       *        could take to move, since we must have it
       *        on screen by then.
       *
       *        One alternative might be to check the result
       *        and if we are still not on screen, then just
       *        pin it to a row-height from the top or bottom.
       */
      g_timeout_add (300,
                     ide_tree_node_show_popover_timeout_cb,
                     popreq);

      return;
    }

  ide_tree_node_show_popover_timeout_cb (g_steal_pointer (&popreq));
}

const gchar *
ide_tree_node_get_tag (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->tag;
}

void
ide_tree_node_set_tag (IdeTreeNode *self,
                       const gchar *tag)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (!ide_str_equal0 (self->tag, tag))
    {
      g_free (self->tag);
      self->tag = g_strdup (tag);
    }
}

gboolean
ide_tree_node_is_tag (IdeTreeNode *self,
                      const gchar *tag)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return tag && ide_str_equal0 (self->tag, tag);
}

void
ide_tree_node_add_emblem (IdeTreeNode *self,
                          GEmblem     *emblem)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  self->emblems = g_list_append (self->emblems, g_object_ref (emblem));
}

GIcon *
_ide_tree_node_apply_emblems (IdeTreeNode *self,
                              GIcon       *base)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  if (self->emblems != NULL)
    {
      g_autoptr(GIcon) emblemed = g_emblemed_icon_new (base, NULL);

      for (const GList *iter = self->emblems; iter; iter = iter->next)
        g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed), iter->data);

      return G_ICON (g_steal_pointer (&emblemed));
    }

  return g_object_ref (base);
}

const GdkRGBA *
ide_tree_node_get_foreground_rgba (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->foreground_set ? &self->foreground : NULL;
}

void
ide_tree_node_set_foreground_rgba (IdeTreeNode   *self,
                                   const GdkRGBA *foreground_rgba)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  self->foreground_set = !!foreground_rgba;

  if (foreground_rgba)
    self->foreground = *foreground_rgba;

  ide_tree_node_emit_changed (self);
}

const GdkRGBA *
ide_tree_node_get_background_rgba (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->background_set ? &self->background : NULL;
}

void
ide_tree_node_set_background_rgba (IdeTreeNode   *self,
                                   const GdkRGBA *background_rgba)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  self->background_set = !!background_rgba;

  if (background_rgba)
    self->background = *background_rgba;

  ide_tree_node_emit_changed (self);
}

void
_ide_tree_node_apply_colors (IdeTreeNode     *self,
                             GtkCellRenderer *cell)
{
  PangoAttrList *attrs = NULL;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->foreground_set)
    {
      if (!attrs)
        attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs,
                              pango_attr_foreground_new (self->foreground.red * 65535,
                                                         self->foreground.green * 65535,
                                                         self->foreground.blue * 65535));
    }

  if (self->background_set)
    {
      if (!attrs)
        attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs,
                              pango_attr_background_new (self->background.red * 65535,
                                                         self->background.green * 65535,
                                                         self->background.blue * 65535));
    }

  g_object_set (cell, "attributes", attrs, NULL);
  g_clear_pointer (&attrs, pango_attr_list_unref);
}

gboolean
ide_tree_node_is_selected (IdeTreeNode *self)
{
  g_autoptr(GtkTreePath) path = NULL;
  GtkTreeSelection *selection;
  IdeTreeModel *model;
  IdeTree *tree;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  if ((path = ide_tree_node_get_path (self)) &&
      (model = ide_tree_node_get_model (self)) &&
      (tree = ide_tree_model_get_tree (model)) &&
      (selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree))))
    return gtk_tree_selection_path_is_selected (selection, path);

  return FALSE;
}

gboolean
ide_tree_node_get_has_error (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->has_error;
}

void
ide_tree_node_set_has_error (IdeTreeNode *self,
                             gboolean     has_error)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  has_error = !!has_error;

  if (has_error != self->has_error)
    {
      self->has_error = has_error;
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ERROR]);
    }
}

gboolean
ide_tree_node_get_use_markup (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->use_markup;
}

void
ide_tree_node_set_use_markup (IdeTreeNode *self,
                              gboolean     use_markup)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  use_markup = !!use_markup;

  if (use_markup != self->use_markup)
    {
      self->use_markup = use_markup;
      ide_tree_node_emit_changed (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_MARKUP]);
    }
}

IdeTreeNodeFlags
ide_tree_node_get_flags (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), 0);

  return self->flags;
}

void
ide_tree_node_set_flags (IdeTreeNode      *self,
                         IdeTreeNodeFlags  flags)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->flags != flags)
    {
      self->flags = flags;
      ide_tree_node_emit_changed (self);
    }
}
