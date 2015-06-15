/* gb-tree-node.c
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

#include "gb-tree.h"
#include "gb-tree-node.h"
#include "gb-tree-private.h"

struct _GbTreeNode
{
  GInitiallyUnowned  parent_instance;

  GObject           *item;
  GbTreeNode        *parent;
  gchar             *text;
  GbTree            *tree;
  GQuark             icon_name;
  guint              use_markup : 1;
  guint              needs_build : 1;
};

typedef struct
{
  GbTreeNode *self;
  GtkPopover *popover;
} PopupRequest;

G_DEFINE_TYPE (GbTreeNode, gb_tree_node, G_TYPE_INITIALLY_UNOWNED)

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_ITEM,
  PROP_PARENT,
  PROP_TEXT,
  PROP_TREE,
  PROP_USE_MARKUP,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * gb_tree_node_new:
 *
 * Creates a new #GbTreeNode instance. This is handy for situations where you
 * do not want to subclass #GbTreeNode.
 *
 * Returns: (transfer full): A #GbTreeNode
 */
GbTreeNode *
gb_tree_node_new (void)
{
  return g_object_new (GB_TYPE_TREE_NODE, NULL);
}

/**
 * gb_tree_node_get_tree:
 * @node: (in): A #GbTreeNode.
 *
 * Fetches the #GbTree instance that owns the node.
 *
 * Returns: (transfer none): A #GbTree.
 */
GbTree *
gb_tree_node_get_tree (GbTreeNode *node)
{
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  return node->tree;
}

/**
 * gb_tree_node_set_tree:
 * @node: (in): A #GbTreeNode.
 * @tree: (in): A #GbTree.
 *
 * Internal method to set the nodes tree.
 */
void
_gb_tree_node_set_tree (GbTreeNode *node,
                        GbTree     *tree)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (!tree || GB_IS_TREE (tree));

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

void
gb_tree_node_insert_sorted (GbTreeNode            *node,
                            GbTreeNode            *child,
                            GbTreeNodeCompareFunc  compare_func,
                            gpointer               user_data)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (GB_IS_TREE_NODE (child));
  g_return_if_fail (compare_func != NULL);

  _gb_tree_insert_sorted (node->tree, node, child, compare_func, user_data);
}

/**
 * gb_tree_node_append:
 * @node: (in): A #GbTreeNode.
 *
 * Appends @child to the list of children owned by @node.
 */
void
gb_tree_node_append (GbTreeNode *node,
                     GbTreeNode *child)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));

  _gb_tree_append (node->tree, node, child);
}

/**
 * gb_tree_node_prepend:
 * @node: (in): A #GbTreeNode.
 *
 * Prepends @child to the list of children owned by @node.
 */
void
gb_tree_node_prepend (GbTreeNode *node,
                      GbTreeNode *child)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));

  _gb_tree_prepend (node->tree, node, child);
}

/**
 * gb_tree_node_remove:
 * @node: (in): A #GbTreeNode.
 *
 * Removes @child from the list of children owned by @node.
 */
void
gb_tree_node_remove (GbTreeNode *node,
                     GbTreeNode *child)
{
  GtkTreeModel *model = NULL;
  GtkTreePath *path;
  GtkTreeIter iter;
  GbTree *tree = NULL;

  g_return_if_fail (GB_IS_TREE_NODE (node));

  tree = gb_tree_node_get_tree (node);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  path = gb_tree_node_get_path (node);

  g_object_ref (tree);
  g_object_ref (model);

  if (gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

  g_clear_object (&model);
  g_clear_object (&tree);

  gtk_tree_path_free (path);
}

/**
 * gb_tree_node_get_path:
 * @node: (in): A #GbTreeNode.
 *
 * Gets a #GtkTreePath for @node.
 *
 * Returns: (nullable) (transfer full): A #GtkTreePath if successful; otherwise %NULL.
 */
GtkTreePath *
gb_tree_node_get_path (GbTreeNode *node)
{
  GbTreeNode *toplevel;
  GtkTreePath *path;
  GList *list = NULL;

  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  if ((node->parent == NULL) || (node->tree == NULL))
    return NULL;

  do
    list = g_list_prepend (list, node);
  while ((node = node->parent));

  toplevel = list->data;

  g_assert (toplevel);
  g_assert (toplevel->tree);

  list = g_list_remove_link (list, list);
  path = _gb_tree_get_path (toplevel->tree, list);

  g_list_free (list);

  return path;
}

gboolean
gb_tree_node_get_iter (GbTreeNode  *self,
                       GtkTreeIter *iter)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  gboolean ret = FALSE;

  g_return_val_if_fail (GB_IS_TREE_NODE (self), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (self->tree != NULL)
    {
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->tree));
      path = gb_tree_node_get_path (self);
      ret = gtk_tree_model_get_iter (model, iter, path);
      gtk_tree_path_free (path);
    }

#if 0
  if (ret)
    {
      GbTreeNode *other = NULL;

      gtk_tree_model_get (model, iter, 0, &other, -1);
      g_assert (other == self);
      g_clear_object (&other);
    }
#endif

  return ret;
}

/**
 * gb_tree_node_get_parent:
 * @node: (in): A #GbTreeNode.
 *
 * Retrieves the parent #GbTreeNode for @node.
 *
 * Returns: (transfer none): A #GbTreeNode.
 */
GbTreeNode *
gb_tree_node_get_parent (GbTreeNode *node)
{
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  return node->parent;
}

/**
 * gb_tree_node_get_icon_name:
 *
 * Fetches the icon-name of the icon to display, or NULL for no icon.
 */
const gchar *
gb_tree_node_get_icon_name (GbTreeNode *node)
{
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  return g_quark_to_string (node->icon_name);
}

/**
 * gb_tree_node_set_icon_name:
 * @node: (in): A #GbTreeNode.
 * @icon_name: (in): The icon name.
 *
 * Sets the icon name of the node. This is displayed in the pixbuf
 * cell of the GbTree.
 */
void
gb_tree_node_set_icon_name (GbTreeNode  *node,
                            const gchar *icon_name)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));

  node->icon_name = g_quark_from_string (icon_name);
  g_object_notify_by_pspec (G_OBJECT (node), gParamSpecs [PROP_ICON_NAME]);
}

/**
 * gb_tree_node_set_item:
 * @node: (in): A #GbTreeNode.
 * @item: (in): A #GObject.
 *
 * An optional object to associate with the node. This is handy to save needing
 * to subclass the #GbTreeNode class.
 */
void
gb_tree_node_set_item (GbTreeNode *node,
                       GObject    *item)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&node->item, item))
    g_object_notify_by_pspec (G_OBJECT (node), gParamSpecs [PROP_ITEM]);
}

void
_gb_tree_node_set_parent (GbTreeNode *node,
                          GbTreeNode *parent)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (node->parent == NULL);
  g_return_if_fail (!parent || GB_IS_TREE_NODE (parent));

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
gb_tree_node_get_text (GbTreeNode *node)
{
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  return node->text;
}

/**
 * gb_tree_node_set_text:
 * @node: (in): A #GbTreeNode.
 * @text: (in): The node text.
 *
 * Sets the text of the node. This is displayed in the text
 * cell of the GbTree.
 */
void
gb_tree_node_set_text (GbTreeNode  *node,
                       const gchar *text)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));

  if (g_strcmp0 (text, node->text) != 0)
    {
      g_free (node->text);
      node->text = g_strdup (text);
      g_object_notify_by_pspec (G_OBJECT (node), gParamSpecs [PROP_TEXT]);
    }
}

static void
gb_tree_node_set_use_markup (GbTreeNode *node,
                             gboolean    use_markup)
{
  g_return_if_fail (GB_IS_TREE_NODE (node));

  node->use_markup = !!use_markup;
  g_object_notify_by_pspec (G_OBJECT (node), gParamSpecs [PROP_USE_MARKUP]);
}

/**
 * gb_tree_node_get_item:
 * @node: (in): A #GbTreeNode.
 *
 * Gets a #GObject for the node, if one was set.
 *
 * Returns: (transfer none): A #GObject or %NULL.
 */
GObject *
gb_tree_node_get_item (GbTreeNode *node)
{
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  return node->item;
}

void
gb_tree_node_expand (GbTreeNode *node,
                     gboolean    expand_ancestors)
{
  GbTree *tree;
  GtkTreePath *path;

  g_return_if_fail (GB_IS_TREE_NODE (node));

  tree = gb_tree_node_get_tree (node);
  path = gb_tree_node_get_path (node);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree), path, FALSE);
  if (expand_ancestors)
    gtk_tree_view_expand_to_path (GTK_TREE_VIEW (tree), path);
  gtk_tree_path_free (path);
}

void
gb_tree_node_collapse (GbTreeNode *node)
{
  GbTree *tree;
  GtkTreePath *path;

  g_return_if_fail (GB_IS_TREE_NODE (node));

  tree = gb_tree_node_get_tree (node);
  path = gb_tree_node_get_path (node);
  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree), path);
  gtk_tree_path_free (path);
}

void
gb_tree_node_select (GbTreeNode  *node)
{
  GbTree *tree;
  GtkTreePath *path;
  GtkTreeSelection *selection;

  g_return_if_fail (GB_IS_TREE_NODE (node));

  tree = gb_tree_node_get_tree (node);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  path = gb_tree_node_get_path (node);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}

void
gb_tree_node_get_area (GbTreeNode   *node,
                       GdkRectangle *area)
{
  GbTree *tree;
  GtkTreeViewColumn *column;
  GtkTreePath *path;

  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (area != NULL);

  tree = gb_tree_node_get_tree (node);
  path = gb_tree_node_get_path (node);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), 0);
  gtk_tree_view_get_cell_area (GTK_TREE_VIEW (tree), path, column, area);
  gtk_tree_path_free (path);
}

void
gb_tree_node_invalidate (GbTreeNode *self)
{
  g_return_if_fail (GB_IS_TREE_NODE (self));

  if (self->tree != NULL)
    _gb_tree_invalidate (self->tree, self);
}

gboolean
gb_tree_node_get_expanded (GbTreeNode *self)
{
  GtkTreePath *path;
  gboolean ret = TRUE;

  g_return_val_if_fail (GB_IS_TREE_NODE (self), FALSE);

  if ((self->tree != NULL) && (self->parent != NULL) && (self->parent->parent != NULL))
    {
      path = gb_tree_node_get_path (self);
      g_assert (path != NULL);
      ret = gtk_tree_view_row_expanded (GTK_TREE_VIEW (self->tree), path);
      gtk_tree_path_free (path);
    }

  return ret;
}

static void
gb_tree_node_finalize (GObject *object)
{
  GbTreeNode *self = GB_TREE_NODE (object);

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

  G_OBJECT_CLASS (gb_tree_node_parent_class)->finalize (object);
}

static void
gb_tree_node_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbTreeNode *node = GB_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, g_quark_to_string (node->icon_name));
      break;

    case PROP_ITEM:
      g_value_set_object (value, node->item);
      break;

    case PROP_PARENT:
      g_value_set_object (value, node->parent);
      break;

    case PROP_TEXT:
      g_value_set_string (value, node->text);
      break;

    case PROP_TREE:
      g_value_set_object (value, gb_tree_node_get_tree (node));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, node->use_markup);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tree_node_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbTreeNode *node = GB_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gb_tree_node_set_icon_name (node, g_value_get_string (value));
      break;

    case PROP_ITEM:
      gb_tree_node_set_item (node, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gb_tree_node_set_text (node, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      gb_tree_node_set_use_markup (node, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tree_node_class_init (GbTreeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_tree_node_finalize;
  object_class->get_property = gb_tree_node_get_property;
  object_class->set_property = gb_tree_node_set_property;

  /**
   * GbTreeNode:icon-name:
   *
   * An icon-name to display on the row.
   */
  gParamSpecs[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         _("Icon Name"),
                         _("The icon name to display."),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GbTreeNode:item:
   *
   * An optional #GObject to associate with the node.
   */
  gParamSpecs[PROP_ITEM] =
    g_param_spec_object ("item",
                         _("Item"),
                         _("Optional object to associate with node."),
                         G_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GbTreeNode:parent:
   *
   * The parent of the node.
   */
  gParamSpecs [PROP_PARENT] =
    g_param_spec_object ("parent",
                         _("Parent"),
                         _("The parent node."),
                         GB_TYPE_TREE_NODE,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GbTreeNode:tree:
   *
   * The tree the node belongs to.
   */
  gParamSpecs [PROP_TREE] =
    g_param_spec_object ("tree",
                         _("Tree"),
                         _("The GbTree the node belongs to."),
                         GB_TYPE_TREE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GbTreeNode:text:
   *
   * Text to display on the tree node.
   */
  gParamSpecs [PROP_TEXT] =
    g_param_spec_string ("text",
                         _("Text"),
                         _("The text of the node."),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GbTreeNode:use-markup:
   *
   * If the "text" property includes #GMarkup.
   */
  gParamSpecs [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup",
                          _("Use Markup"),
                          _("If text should be translated as markup."),
                          FALSE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_tree_node_init (GbTreeNode *node)
{
  node->needs_build = TRUE;
}

static gboolean
gb_tree_node_show_popover_timeout_cb (gpointer data)
{
  PopupRequest *popreq = data;
  GdkRectangle rect;
  GtkAllocation alloc;
  GbTree *tree;

  g_assert (popreq);
  g_assert (GB_IS_TREE_NODE (popreq->self));
  g_assert (GTK_IS_POPOVER (popreq->popover));

  if (!(tree = gb_tree_node_get_tree (popreq->self)))
    goto cleanup;

  gb_tree_node_get_area (popreq->self, &rect);
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
  gtk_widget_show (GTK_WIDGET (popreq->popover));

cleanup:
  g_object_unref (popreq->self);
  g_object_unref (popreq->popover);
  g_free (popreq);

  return G_SOURCE_REMOVE;
}

void
gb_tree_node_show_popover (GbTreeNode *self,
                           GtkPopover *popover)
{
  GdkRectangle cell_area;
  GdkRectangle visible_rect;
  GbTree *tree;
  PopupRequest *popreq;

  g_return_if_fail (GB_IS_TREE_NODE (self));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  tree = gb_tree_node_get_tree (self);
  gtk_tree_view_get_visible_rect (GTK_TREE_VIEW (tree), &visible_rect);
  gb_tree_node_get_area (self, &cell_area);
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

      path = gb_tree_node_get_path (self);
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
                     gb_tree_node_show_popover_timeout_cb,
                     popreq);
    }
  else
    {
      gb_tree_node_show_popover_timeout_cb (popreq);
    }
}

gboolean
_gb_tree_node_get_needs_build (GbTreeNode *self)
{
  g_assert (GB_IS_TREE_NODE (self));

  return self->needs_build;
}

void
_gb_tree_node_set_needs_build (GbTreeNode *self,
                               gboolean    needs_build)
{
  g_assert (GB_IS_TREE_NODE (self));

  self->needs_build = !!needs_build;
}
