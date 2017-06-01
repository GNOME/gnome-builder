/* ide-tree-node.c
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

#define G_LOG_DOMAIN "tree-node"

#include <glib/gi18n.h>

#include "dazzle.h"

#include "ide-tree.h"
#include "ide-tree-node.h"
#include "ide-tree-private.h"

struct _IdeTreeNode
{
  GInitiallyUnowned  parent_instance;

  GObject           *item;
  IdeTreeNode       *parent;
  gchar             *text;
  IdeTree           *tree;
  GQuark             icon_name;
  GIcon             *gicon;
  GList             *emblems;
  guint              use_markup : 1;
  guint              needs_build : 1;
  guint              is_dummy : 1;
  guint              children_possible : 1;
  guint              use_dim_label : 1;
};

typedef struct
{
  IdeTreeNode *self;
  GtkPopover *popover;
} PopupRequest;

G_DEFINE_TYPE (IdeTreeNode, ide_tree_node, G_TYPE_INITIALLY_UNOWNED)
DZL_DEFINE_COUNTER (instances, "IdeTreeNode", "Instances", "Number of IdeTreeNode instances")

enum {
  PROP_0,
  PROP_CHILDREN_POSSIBLE,
  PROP_ICON_NAME,
  PROP_GICON,
  PROP_ITEM,
  PROP_PARENT,
  PROP_TEXT,
  PROP_TREE,
  PROP_USE_DIM_LABEL,
  PROP_USE_MARKUP,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

/**
 * ide_tree_node_new:
 *
 * Creates a new #IdeTreeNode instance. This is handy for situations where you
 * do not want to subclass #IdeTreeNode.
 *
 * Returns: (transfer full): A #IdeTreeNode
 */
IdeTreeNode *
ide_tree_node_new (void)
{
  return g_object_new (IDE_TYPE_TREE_NODE, NULL);
}

/**
 * ide_tree_node_get_tree:
 * @node: (in): A #IdeTreeNode.
 *
 * Fetches the #IdeTree instance that owns the node.
 *
 * Returns: (transfer none): A #IdeTree.
 */
IdeTree *
ide_tree_node_get_tree (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  return node->tree;
}

/**
 * ide_tree_node_set_tree:
 * @node: (in): A #IdeTreeNode.
 * @tree: (in): A #IdeTree.
 *
 * Internal method to set the nodes tree.
 */
void
_ide_tree_node_set_tree (IdeTreeNode *node,
                        IdeTree     *tree)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!tree || IDE_IS_TREE (tree));

  if (node->tree != tree)
    {
      if (node->tree != NULL)
        {
          g_object_remove_weak_pointer (G_OBJECT (node->tree), (gpointer *)&node->tree);
          node->tree = NULL;
        }

      if (tree != NULL)
        {
          node->tree = tree;
          g_object_add_weak_pointer (G_OBJECT (node->tree), (gpointer *)&node->tree);
        }
    }
}

/**
 * ide_tree_node_insert_sorted:
 * @node: A #IdeTreeNode.
 * @child: A #IdeTreeNode.
 * @compare_func: (scope call): A compare func to compare nodes.
 * @user_data: user data for @compare_func.
 *
 * Inserts a @child as a child of @node, sorting it among the other children.
 */
void
ide_tree_node_insert_sorted (IdeTreeNode            *node,
                            IdeTreeNode            *child,
                            IdeTreeNodeCompareFunc  compare_func,
                            gpointer               user_data)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (compare_func != NULL);

  _ide_tree_insert_sorted (node->tree, node, child, compare_func, user_data);
}

/**
 * ide_tree_node_append:
 * @node: A #IdeTreeNode.
 * @child: A #IdeTreeNode.
 *
 * Appends @child to the list of children owned by @node.
 */
void
ide_tree_node_append (IdeTreeNode *node,
                     IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  _ide_tree_append (node->tree, node, child);
}

/**
 * ide_tree_node_prepend:
 * @node: A #IdeTreeNode.
 * @child: A #IdeTreeNode.
 *
 * Prepends @child to the list of children owned by @node.
 */
void
ide_tree_node_prepend (IdeTreeNode *node,
                      IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  _ide_tree_prepend (node->tree, node, child);
}

/**
 * ide_tree_node_remove:
 * @node: A #IdeTreeNode.
 * @child: A #IdeTreeNode.
 *
 * Removes @child from the list of children owned by @node.
 */
void
ide_tree_node_remove (IdeTreeNode *node,
                     IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));

  _ide_tree_remove (node->tree, child);
}

/**
 * ide_tree_node_get_path:
 * @node: (in): A #IdeTreeNode.
 *
 * Gets a #GtkTreePath for @node.
 *
 * Returns: (nullable) (transfer full): A #GtkTreePath if successful; otherwise %NULL.
 */
GtkTreePath *
ide_tree_node_get_path (IdeTreeNode *node)
{
  IdeTreeNode *toplevel;
  GtkTreePath *path;
  GList *list = NULL;

  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  if ((node->parent == NULL) || (node->tree == NULL))
    return NULL;

  do
    {
      list = g_list_prepend (list, node);
    }
  while ((node = node->parent));

  toplevel = list->data;

  g_assert (toplevel);
  g_assert (toplevel->tree);

  path = _ide_tree_get_path (toplevel->tree, list);

  g_list_free (list);

  return path;
}

gboolean
ide_tree_node_get_iter (IdeTreeNode  *self,
                       GtkTreeIter *iter)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (self->tree != NULL)
    ret = _ide_tree_get_iter (self->tree, self, iter);

  return ret;
}

/**
 * ide_tree_node_get_parent:
 * @node: (in): A #IdeTreeNode.
 *
 * Retrieves the parent #IdeTreeNode for @node.
 *
 * Returns: (transfer none): A #IdeTreeNode.
 */
IdeTreeNode *
ide_tree_node_get_parent (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  return node->parent;
}

/**
 * ide_tree_node_get_gicon:
 *
 * Fetch the GIcon, re-render if necessary
 *
 * Returns: (transfer none): An #GIcon or %NULL.
 */
GIcon *
ide_tree_node_get_gicon (IdeTreeNode *self)
{
  const gchar *icon_name;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  icon_name = ide_tree_node_get_icon_name (self);

  if G_UNLIKELY (self->gicon == NULL && icon_name != NULL)
    {
      g_autoptr(GIcon) base = NULL;
      g_autoptr(GIcon) icon = NULL;

      base = g_themed_icon_new (icon_name);
      icon = g_emblemed_icon_new (base, NULL);

      for (GList *iter = self->emblems; iter != NULL; iter = iter->next)
        {
          const gchar *emblem_icon_name = iter->data;
          g_autoptr(GIcon) emblem_base = NULL;
          g_autoptr(GEmblem) emblem = NULL;

          emblem_base = g_themed_icon_new (emblem_icon_name);
          emblem = g_emblem_new (emblem_base);

          g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (icon), emblem);
        }

      if (g_set_object (&self->gicon, icon))
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
    }

  return self->gicon;
}

/**
 * ide_tree_node_get_icon_name:
 *
 * Fetches the icon-name of the icon to display, or NULL for no icon.
 */
const gchar *
ide_tree_node_get_icon_name (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  return g_quark_to_string (node->icon_name);
}

/**
 * ide_tree_node_set_icon_name:
 * @node: A #IdeTreeNode.
 * @icon_name: (nullable): The icon name.
 *
 * Sets the icon name of the node. This is displayed in the pixbuf
 * cell of the IdeTree.
 */
void
ide_tree_node_set_icon_name (IdeTreeNode *node,
                             const gchar *icon_name)
{
  GQuark value = 0;

  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (icon_name != NULL)
    value = g_quark_from_string (icon_name);

  if (value != node->icon_name)
    {
      node->icon_name = value;
      g_clear_object (&node->gicon);
      g_object_notify_by_pspec (G_OBJECT (node), properties [PROP_ICON_NAME]);
      g_object_notify_by_pspec (G_OBJECT (node), properties [PROP_GICON]);
    }
}

/**
 * ide_tree_node_add_emblem:
 * @self: An #IdeTreeNode
 * @emblem_name: the icon-name of the emblem
 *
 * Adds an emplem to be rendered on top of the node.
 *
 * Use ide_tree_node_remove_emblem() to remove an emblem.
 */
void
ide_tree_node_add_emblem (IdeTreeNode *self,
                          const gchar *emblem)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  for (GList *iter = self->emblems; iter != NULL; iter = iter->next)
    {
      const gchar *iter_icon_name = iter->data;

      if (g_strcmp0 (iter_icon_name, emblem) == 0)
        return;
    }

  self->emblems = g_list_prepend (self->emblems, g_strdup (emblem));
  g_clear_object (&self->gicon);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
}

void
ide_tree_node_remove_emblem (IdeTreeNode *self,
                             const gchar *emblem_name)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  for (GList *iter = self->emblems; iter != NULL; iter = iter->next)
    {
      gchar *iter_icon_name = iter->data;

      if (g_strcmp0 (iter_icon_name, emblem_name) == 0)
        {
          g_free (iter_icon_name);
          self->emblems = g_list_delete_link (self->emblems, iter);
          g_clear_object (&self->gicon);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
          return;
        }
    }
}

/**
 * ide_tree_node_clear_emblems:
 *
 * Removes all emblems from @self.
 */
void
ide_tree_node_clear_emblems (IdeTreeNode *self)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  g_list_free_full (self->emblems, g_free);
  self->emblems = NULL;
  g_clear_object (&self->gicon);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
}

/**
 * ide_tree_node_has_emblem:
 * @self: An #IdeTreeNode
 * @emblem_name: a string containing the emblem name
 *
 * Checks to see if @emblem_name has been added to the #IdeTreeNode.
 *
 * Returns: %TRUE if @emblem_name is used by @self
 */
gboolean
ide_tree_node_has_emblem (IdeTreeNode *self,
                          const gchar *emblem_name)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  for (GList *iter = self->emblems; iter != NULL; iter = iter->next)
    {
      const gchar *iter_icon_name = iter->data;

      if (g_strcmp0 (iter_icon_name, emblem_name) == 0)
        return TRUE;
    }

  return FALSE;
}

void
ide_tree_node_set_emblems (IdeTreeNode         *self,
                           const gchar * const *emblems)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->emblems != NULL)
    {
      g_list_free_full (self->emblems, g_free);
      self->emblems = NULL;
    }

  if (emblems != NULL)
    {
      guint len = g_strv_length ((gchar **)emblems);

      for (guint i = len; i > 0; i--)
        self->emblems = g_list_prepend (self->emblems, g_strdup (emblems[i-1]));
    }

  g_clear_object (&self->gicon);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GICON]);
}

/**
 * ide_tree_node_set_item:
 * @node: (in): A #IdeTreeNode.
 * @item: (in): A #GObject.
 *
 * An optional object to associate with the node. This is handy to save needing
 * to subclass the #IdeTreeNode class.
 */
void
ide_tree_node_set_item (IdeTreeNode *node,
                        GObject     *item)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&node->item, item))
    g_object_notify_by_pspec (G_OBJECT (node), properties [PROP_ITEM]);
}

void
_ide_tree_node_set_parent (IdeTreeNode *node,
                           IdeTreeNode *parent)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (node->parent == NULL);
  g_return_if_fail (!parent || IDE_IS_TREE_NODE (parent));

  if (parent != node->parent)
    {
      if (node->parent != NULL)
        {
          g_object_remove_weak_pointer (G_OBJECT (node->parent), (gpointer *)&node->parent);
          node->parent = NULL;
        }

      if (parent != NULL)
        {
          node->parent = parent;
          g_object_add_weak_pointer (G_OBJECT (node->parent), (gpointer *)&node->parent);
        }
    }
}

const gchar *
ide_tree_node_get_text (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  return node->text;
}

/**
 * ide_tree_node_set_text:
 * @node: A #IdeTreeNode.
 * @text: (nullable): The node text.
 *
 * Sets the text of the node. This is displayed in the text
 * cell of the IdeTree.
 */
void
ide_tree_node_set_text (IdeTreeNode *node,
                        const gchar *text)
{
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (g_strcmp0 (text, node->text) != 0)
    {
      g_free (node->text);
      node->text = g_strdup (text);
      g_object_notify_by_pspec (G_OBJECT (node), properties [PROP_TEXT]);
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
                             gboolean    use_markup)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  use_markup = !!use_markup;

  if (self->use_markup != use_markup)
    {
      self->use_markup = use_markup;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_MARKUP]);
    }
}

/**
 * ide_tree_node_get_item:
 * @node: (in): A #IdeTreeNode.
 *
 * Gets a #GObject for the node, if one was set.
 *
 * Returns: (transfer none): A #GObject or %NULL.
 */
GObject *
ide_tree_node_get_item (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  return node->item;
}

gboolean
ide_tree_node_expand (IdeTreeNode *node,
                     gboolean    expand_ancestors)
{
  IdeTree *tree;
  GtkTreePath *path;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);

  tree = ide_tree_node_get_tree (node);
  path = ide_tree_node_get_path (node);
  ret = gtk_tree_view_expand_row (GTK_TREE_VIEW (tree), path, FALSE);
  if (expand_ancestors)
    gtk_tree_view_expand_to_path (GTK_TREE_VIEW (tree), path);
  gtk_tree_path_free (path);

  return ret;
}

void
ide_tree_node_collapse (IdeTreeNode *node)
{
  IdeTree *tree;
  GtkTreePath *path;

  g_return_if_fail (IDE_IS_TREE_NODE (node));

  tree = ide_tree_node_get_tree (node);
  path = ide_tree_node_get_path (node);
  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree), path);
  gtk_tree_path_free (path);
}

void
ide_tree_node_select (IdeTreeNode *node)
{
  IdeTree *tree;
  GtkTreePath *path;
  GtkTreeSelection *selection;

  g_return_if_fail (IDE_IS_TREE_NODE (node));

  tree = ide_tree_node_get_tree (node);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  path = ide_tree_node_get_path (node);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}

void
ide_tree_node_get_area (IdeTreeNode  *node,
                        GdkRectangle *area)
{
  IdeTree *tree;
  GtkTreeViewColumn *column;
  GtkTreePath *path;

  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (area != NULL);

  tree = ide_tree_node_get_tree (node);
  path = ide_tree_node_get_path (node);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), 0);
  gtk_tree_view_get_cell_area (GTK_TREE_VIEW (tree), path, column, area);
  gtk_tree_path_free (path);
}

void
ide_tree_node_invalidate (IdeTreeNode *self)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->tree != NULL)
    _ide_tree_invalidate (self->tree, self);
}

gboolean
ide_tree_node_get_expanded (IdeTreeNode *self)
{
  GtkTreePath *path;
  gboolean ret = TRUE;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  if ((self->tree != NULL) && (self->parent != NULL))
    {
      path = ide_tree_node_get_path (self);
      ret = gtk_tree_view_row_expanded (GTK_TREE_VIEW (self->tree), path);
      gtk_tree_path_free (path);
    }

  return ret;
}

static void
ide_tree_node_finalize (GObject *object)
{
  IdeTreeNode *self = IDE_TREE_NODE (object);

  g_clear_object (&self->item);
  g_clear_pointer (&self->text, g_free);

  if (self->tree)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->tree), (gpointer *)&self->tree);
      self->tree = NULL;
    }

  if (self->parent)
    {
      g_object_remove_weak_pointer (G_OBJECT (self->parent),
                                    (gpointer *)&self->parent);
      self->parent = NULL;
    }

  G_OBJECT_CLASS (ide_tree_node_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}

static void
ide_tree_node_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeTreeNode *node = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      g_value_set_boolean (value, ide_tree_node_get_children_possible (node));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, g_quark_to_string (node->icon_name));
      break;

    case PROP_ITEM:
      g_value_set_object (value, node->item);
      break;

    case PROP_GICON:
      g_value_set_object (value, node->gicon);
      break;

    case PROP_PARENT:
      g_value_set_object (value, node->parent);
      break;

    case PROP_TEXT:
      g_value_set_string (value, node->text);
      break;

    case PROP_TREE:
      g_value_set_object (value, ide_tree_node_get_tree (node));
      break;

    case PROP_USE_DIM_LABEL:
      g_value_set_boolean (value, node->use_dim_label);
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, node->use_markup);
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
  IdeTreeNode *node = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      ide_tree_node_set_children_possible (node, g_value_get_boolean (value));
      break;

    case PROP_ICON_NAME:
      ide_tree_node_set_icon_name (node, g_value_get_string (value));
      break;

    case PROP_ITEM:
      ide_tree_node_set_item (node, g_value_get_object (value));
      break;

    case PROP_TEXT:
      ide_tree_node_set_text (node, g_value_get_string (value));
      break;

    case PROP_USE_DIM_LABEL:
      ide_tree_node_set_use_dim_label (node, g_value_get_boolean (value));
      break;

    case PROP_USE_MARKUP:
      ide_tree_node_set_use_markup (node, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_node_class_init (IdeTreeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_tree_node_finalize;
  object_class->get_property = ide_tree_node_get_property;
  object_class->set_property = ide_tree_node_set_property;

  /**
   * IdeTreeNode:children-possible:
   *
   * This property allows for more lazy loading of nodes.
   *
   * When a node becomes visible, we normally build its children nodes
   * so that we know if we need an expansion arrow. However, that can
   * be expensive when rendering directories with lots of subdirectories.
   *
   * Using this, you can always show an arrow without building the children
   * and simply hide the arrow if there were in fact no children (upon
   * expansion).
   */
  properties [PROP_CHILDREN_POSSIBLE] =
    g_param_spec_boolean ("children-possible",
                          "Children Possible",
                          "Allows for lazy creation of children nodes.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));



  /**
   * IdeTreeNode:icon-name:
   *
   * An icon-name to display on the row.
   */
  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon name to display.",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  /**
   * IdeTreeNode:gicon:
   *
   * The cached GIcon to display.
   */
  properties[PROP_GICON] =
    g_param_spec_object ("gicon",
                         "GIcon",
                         "The GIcon object",
                         G_TYPE_ICON,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeTreeNode:item:
   *
   * An optional #GObject to associate with the node.
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "Optional object to associate with node.",
                         G_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeTreeNode:parent:
   *
   * The parent of the node.
   */
  properties [PROP_PARENT] =
    g_param_spec_object ("parent",
                         "Parent",
                         "The parent node.",
                         IDE_TYPE_TREE_NODE,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeTreeNode:tree:
   *
   * The tree the node belongs to.
   */
  properties [PROP_TREE] =
    g_param_spec_object ("tree",
                         "Tree",
                         "The IdeTree the node belongs to.",
                         IDE_TYPE_TREE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeTreeNode:text:
   *
   * Text to display on the tree node.
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text of the node.",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * IdeTreeNode:use-markup:
   *
   * If the "text" property includes #GMarkup.
   */
  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup",
                          "Use Markup",
                          "If text should be translated as markup.",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_USE_DIM_LABEL] =
    g_param_spec_boolean ("use-dim-label",
                          "Use Dim Label",
                          "If text should be rendered with a dim label.",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_tree_node_init (IdeTreeNode *node)
{
  DZL_COUNTER_INC (instances);

  node->needs_build = TRUE;
}

static gboolean
ide_tree_node_show_popover_timeout_cb (gpointer data)
{
  PopupRequest *popreq = data;
  GdkRectangle rect;
  GtkAllocation alloc;
  IdeTree *tree;

  g_assert (popreq);
  g_assert (IDE_IS_TREE_NODE (popreq->self));
  g_assert (GTK_IS_POPOVER (popreq->popover));

  if (!(tree = ide_tree_node_get_tree (popreq->self)))
    goto cleanup;

  ide_tree_node_get_area (popreq->self, &rect);
  gtk_widget_get_allocation (GTK_WIDGET (tree), &alloc);

  if ((rect.x + rect.width) > (alloc.x + alloc.width))
    rect.width = (alloc.x + alloc.width) - rect.x;

  /*
   * FIXME: Wouldn't this be better placed in a theme?
   */
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

  gtk_popover_set_relative_to (popreq->popover, GTK_WIDGET (tree));
  gtk_popover_set_pointing_to (popreq->popover, &rect);
  gtk_popover_popup (popreq->popover);

cleanup:
  g_object_unref (popreq->self);
  g_object_unref (popreq->popover);
  g_free (popreq);

  return G_SOURCE_REMOVE;
}

void
ide_tree_node_show_popover (IdeTreeNode *self,
                           GtkPopover *popover)
{
  GdkRectangle cell_area;
  GdkRectangle visible_rect;
  IdeTree *tree;
  PopupRequest *popreq;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  tree = ide_tree_node_get_tree (self);
  gtk_tree_view_get_visible_rect (GTK_TREE_VIEW (tree), &visible_rect);
  ide_tree_node_get_area (self, &cell_area);
  gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW (tree),
                                                   cell_area.x,
                                                   cell_area.y,
                                                   &cell_area.x,
                                                   &cell_area.y);

  popreq = g_new0 (PopupRequest, 1);
  popreq->self = g_object_ref (self);
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
      gtk_tree_path_free (path);

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
    }
  else
    {
      ide_tree_node_show_popover_timeout_cb (popreq);
    }
}

gboolean
_ide_tree_node_get_needs_build (IdeTreeNode *self)
{
  g_assert (IDE_IS_TREE_NODE (self));

  return self->needs_build;
}

void
_ide_tree_node_set_needs_build (IdeTreeNode *self,
                               gboolean    needs_build)
{
  g_assert (IDE_IS_TREE_NODE (self));

  self->needs_build = !!needs_build;

  if (!needs_build)
    self->is_dummy = FALSE;
}

void
_ide_tree_node_add_dummy_child (IdeTreeNode *self)
{
  GtkTreeStore *model;
  IdeTreeNode *dummy;
  GtkTreeIter iter;
  GtkTreeIter parent;

  g_assert (IDE_IS_TREE_NODE (self));

  model = _ide_tree_get_store (self->tree);
  ide_tree_node_get_iter (self, &parent);
  dummy = g_object_ref_sink (ide_tree_node_new ());
  gtk_tree_store_insert_with_values (model, &iter, &parent, -1,
                                     0, dummy,
                                     -1);
  g_object_unref (dummy);
}

void
_ide_tree_node_remove_dummy_child (IdeTreeNode *self)
{
  GtkTreeStore *model;
  GtkTreeIter iter;
  GtkTreeIter children;

  g_assert (IDE_IS_TREE_NODE (self));

  if (self->parent == NULL)
    return;

  model = _ide_tree_get_store (self->tree);

  if (ide_tree_node_get_iter (self, &iter) &&
      gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &children, &iter))
    {
      while (gtk_tree_store_remove (model, &children)) { }
    }
}

gboolean
ide_tree_node_get_children_possible (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->children_possible;
}

/**
 * ide_tree_node_set_children_possible:
 * @self: A #IdeTreeNode.
 * @children_possible: If the node has children.
 *
 * If the node has not yet been built, setting this to %TRUE will add a
 * dummy child node. This dummy node will be removed when when the node
 * is built by the registered #IdeTreeBuilder instances.
 */
void
ide_tree_node_set_children_possible (IdeTreeNode *self,
                                    gboolean    children_possible)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  children_possible = !!children_possible;

  if (children_possible != self->children_possible)
    {
      self->children_possible = children_possible;

      if (self->tree && self->needs_build)
        {
          if (self->children_possible)
            _ide_tree_node_add_dummy_child (self);
          else
            _ide_tree_node_remove_dummy_child (self);
        }
    }
}

gboolean
ide_tree_node_get_use_dim_label (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->use_dim_label;
}

void
ide_tree_node_set_use_dim_label (IdeTreeNode *self,
                                gboolean    use_dim_label)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  use_dim_label = !!use_dim_label;

  if (use_dim_label != self->use_dim_label)
    {
      self->use_dim_label = use_dim_label;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_DIM_LABEL]);
    }
}

gboolean
ide_tree_node_is_root (IdeTreeNode *node)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);

  return node->parent == NULL;
}
