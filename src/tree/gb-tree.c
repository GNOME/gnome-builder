/* gb-tree.c
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

#define G_LOG_DOMAIN "tree"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-tree.h"
#include "gb-tree-node.h"

struct _GbTreePrivate
{
  GPtrArray    *builders;
  GMenu        *menu;
  GbTreeNode   *root;
  GbTreeNode   *selection;
  GtkTreeStore *store;
  guint         building : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbTree, gb_tree, GTK_TYPE_TREE_VIEW)

enum {
  PROP_0,
  PROP_MENU,
  PROP_ROOT,
  PROP_SELECTION,
  LAST_PROP
};

extern void _gb_tree_node_set_tree (GbTreeNode *node,
                                    GbTree     *tree);

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * gb_tree_get_menu:
 * @tree: (in): A #GbTree.
 *
 * Gets the #GMenu to be displayed when right clicking on an item in the tree.
 * Node's should update actions sensitivity in their selection callbacks.
 *
 * Returns: (transfer none): A #GMenu or %NULL.
 */
GMenu *
gb_tree_get_menu (GbTree *tree)
{
  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

  return tree->priv->menu;
}

/**
 * gb_tree_set_menu:
 * @menu: (in) (transfer none): A #GMenu or %NULL.
 *
 * Set the menu to be used when a popup is to be shown.
 */
void
gb_tree_set_menu (GbTree *tree,
                  GMenu  *menu)
{
  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (!menu || G_IS_MENU (menu));

  if (menu != tree->priv->menu)
    {
      g_clear_object (&tree->priv->menu);
      tree->priv->menu = menu ? g_object_ref (menu) : NULL;
      g_object_notify_by_pspec (G_OBJECT (tree), gParamSpecs [PROP_MENU]);
    }
}

/**
 * gb_tree_unselect:
 * @tree: (in): A #GbTree.
 *
 * Unselects the current item in the tree.
 */
static void
gb_tree_unselect (GbTree *tree)
{
  GtkTreeSelection *selection;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_unselect_all (selection);

  EXIT;
}

/**
 * gb_tree_select:
 * @tree: (in): A #GbTree.
 * @node: (in): A #GbTreeNode.
 *
 * Selects @node within the tree.
 */
static void
gb_tree_select (GbTree     *tree,
                GbTreeNode *node)
{
  GtkTreeSelection *selection;
  GbTreePrivate *priv;
  GtkTreePath *path;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  priv = tree->priv;

  if (priv->selection)
    {
      gb_tree_unselect (tree);
      g_assert (!priv->selection);
    }

  priv->selection = node;

  path = gb_tree_node_get_path (node);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  EXIT;
}

static void
gb_tree_menu_position_func (GtkMenu  *menu,
                            gint     *x,
                            gint     *y,
                            gboolean *push_in,
                            gpointer  user_data)
{
  GdkPoint *loc = user_data;

  g_return_if_fail (loc != NULL);

  if ((loc->x != -1) && (loc->y != -1))
    {
      *x = loc->x;
      *y = loc->y;
    }
}

static void
check_visible_foreach (GtkWidget *widget,
                       gpointer   user_data)
{
  gboolean *at_least_one_visible = user_data;

  if (gtk_widget_get_visible (widget))
    *at_least_one_visible = TRUE;
}

static void
gb_tree_popup (GbTree         *tree,
               GbTreeNode     *node,
               GdkEventButton *button,
               gint            target_x,
               gint            target_y)
{
  GbTreePrivate *priv;
  GbTreeBuilder *builder;
  GtkWidget *menu;
  GdkPoint loc = { -1, -1 };
  gboolean at_least_one_visible = FALSE;
  guint i;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (button != NULL);

  priv = tree->priv;

  menu = gtk_menu_new_from_model (G_MENU_MODEL (priv->menu));

  for (i = 0; i < priv->builders->len; i++)
    {
      builder = g_ptr_array_index (priv->builders, i);
      gb_tree_builder_node_popup (builder, node);
    }

  if ((target_x >= 0) &&  (target_y >= 0))
    {
      gdk_window_get_root_coords (gtk_widget_get_window (GTK_WIDGET (tree)),
                                  target_x, target_y, &loc.x, &loc.y);
      loc.x -= 12;
    }

  gtk_container_foreach (GTK_CONTAINER (menu),
                         check_visible_foreach,
                         &at_least_one_visible);

  if (at_least_one_visible)
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    gb_tree_menu_position_func, &loc,
                    button->button,
                    button->time);

  EXIT;
}

/**
 * gb_tree_selection_changed:
 * @tree: (in): A #GbTree.
 *
 * Handle the selection changing.
 */
static void
gb_tree_selection_changed (GbTree           *tree,
                           GtkTreeSelection *selection)
{
  GbTreePrivate *priv;
  GbTreeBuilder *builder;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *node;
  GbTreeNode *unselection;
  gint i;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  priv = tree->priv;

  if ((unselection = priv->selection))
    {
      priv->selection = NULL;
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          gb_tree_builder_node_unselected (builder, unselection);
        }
    }

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &node, -1);
      if (node)
        {
          for (i = 0; i < priv->builders->len; i++)
            {
              builder = g_ptr_array_index (priv->builders, i);
              gb_tree_builder_node_selected (builder, node);
            }
          g_object_unref (node);
        }
    }

  EXIT;
}

/**
 * gb_tree_get_selected:
 * @tree: (in): A #GbTree.
 *
 * Gets the currently selected node in the tree.
 *
 * Returns: (transfer none): A #GbTreeNode.
 */
GbTreeNode *
gb_tree_get_selected (GbTree *tree)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *ret = NULL;

  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &ret, -1);

      /*
       * We incurred an extra reference when extracting the value from
       * the treemodel. Since it owns the reference, we can drop it here
       * so that we don't transfer the ownership to the caller.
       */
      g_object_unref (ret);
    }

  return ret;
}

/**
 * gb_tree_get_iter_for_node:
 * @tree: (in): A #GbTree.
 * @parent: (in) (allow-none): A #GtkTreeIter of parent or %NULL.
 * @iter: (out): A location for a #GtkTreeIter.
 * @node: (in): A #GbTreeNode expected to be within @parent children.
 *
 * Looks for the #GtkTreeIter that contains @node within the children
 * of @parent. If that item is found, @iter is set and %TRUE is returned.
 * Otherwise, %FALSE is returned.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 */
static gboolean
gb_tree_get_iter_for_node (GbTree      *tree,
                           GtkTreeIter *parent,
                           GtkTreeIter *iter,
                           GbTreeNode  *node)
{
  GbTreePrivate *priv;
  GtkTreeModel *model;
  GbTreeNode *that = NULL;
  gboolean ret;

  ENTRY;

  g_return_val_if_fail (GB_IS_TREE (tree), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GB_IS_TREE_NODE (node), FALSE);

  priv = tree->priv;
  model = GTK_TREE_MODEL (priv->store);

  if (parent)
    ret = gtk_tree_model_iter_children (model, iter, parent);
  else
    ret = gtk_tree_model_get_iter_first (model, iter);

  if (ret)
    {
      do
        {
          gtk_tree_model_get (model, iter, 0, &that, -1);
          if (that == node)
            {
              g_clear_object (&that);
              RETURN (TRUE);
            }
          g_clear_object (&that);
        }
      while (gtk_tree_model_iter_next (model, iter));
    }

  RETURN (FALSE);
}

/**
 * gb_tree_get_path:
 * @tree: (in): A #GbTree.
 * @list: (in) (element-type GbTreeNode): A list of #GbTreeNode.
 *
 * Retrieves the GtkTreePath for a list of GbTreeNode.
 *
 * Returns: (transfer full): A #GtkTreePath.
 */
GtkTreePath *
gb_tree_get_path (GbTree *tree,
                  GList  *list)
{
  GbTreePrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter old_iter;
  GtkTreeIter *parent = NULL;

  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

  priv = tree->priv;
  model = GTK_TREE_MODEL (priv->store);

  if (!list || !gtk_tree_model_get_iter_first (model, &iter))
    return NULL;

  if (list->data == priv->root)
    list = list->next;

  while (gb_tree_get_iter_for_node (tree, parent, &iter, list->data))
    {
      old_iter = iter;
      parent = &old_iter;
      if (list->next)
        list = list->next;
      else
        return gtk_tree_model_get_path (model, &iter);
    }

  return NULL;
}

static gboolean
gb_tree_add_builder_foreach_cb (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      user_data)
{
  GbTreeNode *node = NULL;
  GbTreeBuilder *builder = (GbTreeBuilder *) user_data;

  ENTRY;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_tree_model_get (model, iter, 0, &node, -1);
  gb_tree_builder_build_node (builder, node);
  g_clear_object (&node);

  RETURN (FALSE);
}

/**
 * gb_tree_add_builder:
 * @tree: (in): A #GbTree.
 * @builder: (in) (transfer full): A #GbTreeBuilder to add.
 *
 * Removes a builder from the tree.
 */
void
gb_tree_add_builder (GbTree        *tree,
                     GbTreeBuilder *builder)
{
  GbTreePrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));

  priv = tree->priv;

  g_object_set (builder, "tree", tree, NULL);
  g_ptr_array_add (priv->builders, g_object_ref_sink (builder));
  priv->building = TRUE;
  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                          gb_tree_add_builder_foreach_cb,
                          builder);
  priv->building = FALSE;

  if (GB_TREE_BUILDER_GET_CLASS (builder)->added)
    GB_TREE_BUILDER_GET_CLASS (builder)->added (builder, GTK_WIDGET (tree));

  EXIT;
}

/**
 * gb_tree_remove_builder:
 * @tree: (in): A #GbTree.
 * @builder: (in): A #GbTreeBuilder to remove.
 *
 * Removes a builder from the tree.
 */
void
gb_tree_remove_builder (GbTree        *tree,
                        GbTreeBuilder *builder)
{
  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));

  if (GB_TREE_BUILDER_GET_CLASS (builder)->removed)
    GB_TREE_BUILDER_GET_CLASS (builder)->removed (builder, GTK_WIDGET (tree));

  g_ptr_array_remove (tree->priv->builders, builder);

  EXIT;
}

/**
 * gb_tree_get_root:
 *
 * Retrieves the root node of the tree. The root node is not a visible node
 * in the tree, but a placeholder for all other builders to build upon.
 *
 * Returns: (transfer none) (nullable): A #GbTreeNode or %NULL.
 */
GbTreeNode *
gb_tree_get_root (GbTree *tree)
{
  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

  return tree->priv->root;
}

/**
 * gb_tree_set_root:
 * @tree: (in): A #GbTree.
 * @node: (in): A #GbTreeNode.
 *
 * Sets the root node of the #GbTree widget. This is used to build
 * the items within the treeview. The item itself will not be added
 * to the tree, but the direct children will be.
 */
void
gb_tree_set_root (GbTree     *tree,
                  GbTreeNode *root)
{
  GbTreePrivate *priv;
  GbTreeBuilder *builder;
  gint i;

  ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));

  priv = tree->priv;

  gtk_tree_store_clear (priv->store);
  g_clear_object (&priv->root);

  if (root)
    {
      priv->root = g_object_ref_sink (root);
      _gb_tree_node_set_tree (root, tree);
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          gb_tree_builder_build_node (builder, root);
        }
    }

  EXIT;
}

void
gb_tree_rebuild (GbTree *tree)
{
  GbTreePrivate *priv;
  GbTreeNode *root;

  g_return_if_fail (GB_IS_TREE (tree));

  priv = tree->priv;

  if ((root = priv->root ? g_object_ref (priv->root) : NULL))
    {
      gb_tree_set_root (tree, root);
      g_object_unref (root);
    }
}

/**
 * pixbuf_func:
 * @cell_layout: (in): A #GtkCellRendererPixbuf.
 *
 * Handle preparing a pixbuf cell renderer for drawing.
 */
static void
pixbuf_func (GtkCellLayout   *cell_layout,
             GtkCellRenderer *cell,
             GtkTreeModel    *tree_model,
             GtkTreeIter     *iter,
             gpointer         data)
{
  const gchar *icon_name;
  GbTreeNode *node;

  gtk_tree_model_get (tree_model, iter, 0, &node, -1);
  icon_name = node ? gb_tree_node_get_icon_name (node) : NULL;
  g_object_set (cell, "icon-name", icon_name, NULL);
  g_clear_object (&node);
}

/**
 * text_func:
 * @cell_layout: (in): A #GtkCellRendererText.
 *
 * Handle preparing a text cell renderer for drawing.
 */
static void
text_func (GtkCellLayout   *cell_layout,
           GtkCellRenderer *cell,
           GtkTreeModel    *tree_model,
           GtkTreeIter     *iter,
           gpointer         data)
{
  gboolean use_markup = FALSE;
  GbTreeNode *node = NULL;
  gchar *text = NULL;

  gtk_tree_model_get (tree_model, iter, 0, &node, -1);

  if (node)
    {
      g_object_get (node,
                    "text", &text,
                    "use-markup", &use_markup,
                    NULL);
      g_object_set (cell,
                    use_markup ? "markup" : "text", text,
                    NULL);
      g_free (text);
    }
}

/**
 * gb_tree_add:
 * @tree: (in): A #GbTree.
 * @node: (in): A #GbTreeNode.
 * @child: (in): A #GbTreeNode.
 * @prepend: (in): Should we prepend instead of append?
 *
 * Prepends or appends @child to @node within the #GbTree.
 */
static void
gb_tree_add (GbTree     *tree,
             GbTreeNode *node,
             GbTreeNode *child,
             gboolean    prepend)
{
  GbTreePrivate *priv;
  GbTreeBuilder *builder;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter that;
  gint i;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (GB_IS_TREE_NODE (child));

  priv = tree->priv;

  g_object_set (child, "parent", node, NULL);

  if ((path = gb_tree_node_get_path (node)))
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
      if (prepend)
        gtk_tree_store_prepend (priv->store, &that, &iter);
      else
        gtk_tree_store_append (priv->store, &that, &iter);
      gtk_tree_store_set (priv->store, &that, 0, child, -1);
      gtk_tree_path_free (path);
    }
  else
    {
      if (prepend)
        gtk_tree_store_prepend (priv->store, &iter, NULL);
      else
        gtk_tree_store_append (priv->store, &iter, NULL);
      gtk_tree_store_set (priv->store, &iter, 0, child, -1);
    }

  if (!priv->building)
    {
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          gb_tree_builder_build_node (builder, child);
        }
    }
}

/**
 * gb_tree_append:
 * @tree: (in): A #GbTree.
 * @node: (in): A #GbTreeNode.
 * @child: (in): A #GbTreeNode.
 *
 * Appends @child to @node within the #GbTree.
 */
void
gb_tree_append (GbTree     *tree,
                GbTreeNode *node,
                GbTreeNode *child)
{
  gb_tree_add (tree, node, child, FALSE);
}

/**
 * gb_tree_prepend:
 * @tree: (in): A #GbTree.
 * @node: (in): A #GbTreeNode.
 * @child: (in): A #GbTreeNode.
 *
 * Appends @child to @node within the #GbTree.
 */
void
gb_tree_prepend (GbTree     *tree,
                 GbTreeNode *node,
                 GbTreeNode *child)
{
  gb_tree_add (tree, node, child, TRUE);
}

/**
 * gb_tree_row_activated:
 * @tree_view: (in): A #GbTree.
 * @path: (in): A #GtkTreePath.
 *
 * Handle the row being activated. Expand the row or collapse it.
 */
static void
gb_tree_row_activated (GtkTreeView *tree_view,
                       GtkTreePath *path)
{
  GbTreeBuilder *builder;
  GbTreePrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *node = NULL;
  gboolean handled = FALSE;
  GbTree *tree = (GbTree *) tree_view;
  gint i;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (path != NULL);

  priv = tree->priv;
  model = GTK_TREE_MODEL (priv->store);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gtk_tree_model_get (model, &iter, 0, &node, -1);
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          if ((handled = gb_tree_builder_node_activated (builder, node)))
            break;
        }
      g_clear_object (&node);
    }

  if (!handled)
    {
      if (gtk_tree_view_row_expanded (tree_view, path))
        gtk_tree_view_collapse_row (tree_view, path);
      else
        gtk_tree_view_expand_to_path (tree_view, path);
    }
}

static gboolean
gb_tree_button_press_event (GbTree         *tree,
                            GdkEventButton *button,
                            gpointer        user_data)
{
  GtkAllocation alloc;
  GbTreePrivate *priv;
  GtkTreePath *tree_path = NULL;
  GtkTreeIter iter;
  GbTreeNode *node = NULL;
  gint cell_y;

  g_return_val_if_fail (GB_IS_TREE (tree), FALSE);

  priv = tree->priv;

  if ((button->type == GDK_BUTTON_PRESS) && (button->button == 3))
    {
      gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree),
                                     button->x,
                                     button->y,
                                     &tree_path,
                                     NULL,
                                     NULL,
                                     &cell_y);
      if (!tree_path)
        gb_tree_unselect (tree);
      else
        {
          gtk_widget_get_allocation (GTK_WIDGET (tree), &alloc);
          gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, tree_path);
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, 0, &node, -1);
          gb_tree_select (tree, node);
          gb_tree_popup (tree, node, button,
                         alloc.x + alloc.width,
                         button->y - cell_y);
          g_object_unref (node);
          gtk_tree_path_free (tree_path);
        }
      return TRUE;
    }

  return FALSE;
}

/**
 * gb_tree_finalize:
 * @object: (in): A #GbTree.
 *
 * Finalizer for a #GbTree instance.  Frees any resources held by
 * the instance.
 */
static void
gb_tree_finalize (GObject *object)
{
  GbTreePrivate *priv = GB_TREE (object)->priv;

  g_ptr_array_unref (priv->builders);
  g_clear_object (&priv->menu);
  g_clear_object (&priv->store);
  g_clear_object (&priv->root);

  G_OBJECT_CLASS (gb_tree_parent_class)->finalize (object);
}

/**
 * gb_tree_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gb_tree_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GbTree *tree = GB_TREE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_object (value, tree->priv->root);
      break;

    case PROP_SELECTION:
      g_value_set_object (value, tree->priv->selection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/**
 * gb_tree_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gb_tree_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GbTree *tree = GB_TREE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      gb_tree_set_root (tree, g_value_get_object (value));
      break;

    case PROP_SELECTION:
      gb_tree_select (tree, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/**
 * gb_tree_class_init:
 * @klass: (in): A #GbTreeClass.
 *
 * Initializes the #GbTreeClass and prepares the vtable.
 */
static void
gb_tree_class_init (GbTreeClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_tree_finalize;
  object_class->get_property = gb_tree_get_property;
  object_class->set_property = gb_tree_set_property;

  gParamSpecs[PROP_ROOT] =
    g_param_spec_object ("root",
                         _ ("Root"),
                         _ ("The root object of the tree."),
                         GB_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ROOT,
                                   gParamSpecs[PROP_ROOT]);

  gParamSpecs[PROP_SELECTION] =
    g_param_spec_object ("selection",
                         _ ("Selection"),
                         _ ("The node selection."),
                         GB_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SELECTION,
                                   gParamSpecs[PROP_SELECTION]);
}

/**
 * gb_tree_init:
 * @tree: (in): A #GbTree.
 *
 * Initializes the newly created #GbTree instance.
 */
static void
gb_tree_init (GbTree *tree)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkCellLayout *column;

  tree->priv = gb_tree_get_instance_private (tree);

  tree->priv->builders = g_ptr_array_new ();
  g_ptr_array_set_free_func (tree->priv->builders, g_object_unref);
  tree->priv->store = gtk_tree_store_new (1, GB_TYPE_TREE_NODE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (gb_tree_selection_changed),
                            tree);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "title", "Node",
                         NULL);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF, NULL);
  gtk_cell_layout_pack_start (column, cell, FALSE);
  gtk_cell_layout_set_cell_data_func (column, cell, pixbuf_func, NULL, NULL);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       NULL);
  gtk_cell_layout_pack_start (column, cell, TRUE);
  gtk_cell_layout_set_cell_data_func (column, cell, text_func, NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree),
                               GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree),
                           GTK_TREE_MODEL (tree->priv->store));

  g_signal_connect (tree, "row-activated",
                    G_CALLBACK (gb_tree_row_activated),
                    NULL);
  g_signal_connect (tree, "button-press-event",
                    G_CALLBACK (gb_tree_button_press_event),
                    NULL);
}
