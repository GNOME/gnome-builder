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
#include <ide.h>

#include "gb-tree.h"
#include "gb-tree-node.h"
#include "gb-tree-private.h"
#include "gb-widget.h"

typedef struct
{
  GPtrArray         *builders;
  GbTreeNode        *root;
  GbTreeNode        *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *cell_pixbuf;
  GtkCellRenderer   *cell_text;
  GtkTreeStore      *store;
  guint              show_icons : 1;
  guint              building;
} GbTreePrivate;

typedef struct
{
  gpointer    key;
  GEqualFunc  equal_func;
  GbTreeNode *result;
} NodeLookup;

static void gb_tree_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GbTree, gb_tree, GTK_TYPE_TREE_VIEW,
                         G_ADD_PRIVATE (GbTree)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gb_tree_buildable_init))

enum {
  PROP_0,
  PROP_ROOT,
  PROP_SELECTION,
  PROP_SHOW_ICONS,
  LAST_PROP
};

enum {
  ACTION,
  POPULATE_POPUP,
  LAST_SIGNAL
};

static GtkBuildableIface *gb_tree_parent_buildable_iface;
static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

gboolean
gb_tree_get_show_icons (GbTree *tree)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_val_if_fail (GB_IS_TREE (tree), FALSE);

  return priv->show_icons;
}

void
gb_tree_set_show_icons (GbTree   *tree,
                        gboolean  show_icons)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_if_fail (GB_IS_TREE (tree));

  show_icons = !!show_icons;

  if (show_icons != priv->show_icons)
    {
      priv->show_icons = show_icons;
      g_object_set (priv->cell_pixbuf, "visible", show_icons, NULL);
      /*
       * WORKAROUND:
       *
       * Changing the visibility of the cell does not force a redraw of the
       * tree view. So to force it, we will hide/show our entire pixbuf/text
       * column.
       */
      gtk_tree_view_column_set_visible (priv->column, FALSE);
      gtk_tree_view_column_set_visible (priv->column, TRUE);
      g_object_notify_by_pspec (G_OBJECT (tree),
                                gParamSpecs [PROP_SHOW_ICONS]);
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

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_unselect_all (selection);

  IDE_EXIT;
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
  GtkTreePath *path;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));

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

  IDE_EXIT;
}

static guint
gb_tree_get_row_height (GbTree *tree)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);
  guint extra_padding;
  gint pix_min_height;
  gint pix_nat_height;
  gint text_min_height;
  gint text_nat_height;

  g_assert (GB_IS_TREE (tree));

  gtk_widget_style_get (GTK_WIDGET (tree),
                        "vertical-separator", &extra_padding,
                        NULL);

  gtk_cell_renderer_get_preferred_height (priv->cell_pixbuf,
                                          GTK_WIDGET (tree),
                                          &pix_min_height,
                                          &pix_nat_height);
  gtk_cell_renderer_get_preferred_height (priv->cell_text,
                                          GTK_WIDGET (tree),
                                          &text_min_height,
                                          &text_nat_height);

  return MAX (pix_nat_height, text_nat_height) + extra_padding;
}

static void
gb_tree_menu_position_func (GtkMenu  *menu,
                            gint     *x,
                            gint     *y,
                            gboolean *push_in,
                            gpointer  user_data)
{
  GdkPoint *loc = user_data;
  GtkRequisition req;
  GdkRectangle rect;
  GdkScreen *screen;
  gint monitor;

  g_return_if_fail (loc != NULL);

  gtk_widget_get_preferred_size (GTK_WIDGET (menu), NULL, &req);
  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor = gdk_screen_get_monitor_at_point (screen, *x, *y);
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);

  if ((loc->x != -1) && (loc->y != -1))
    {
      if ((loc->y + req.height) <= (rect.y + rect.height))
        {
          *x = loc->x;
          *y = loc->y;
        }
      else
        {
          GtkWidget *attached;
          guint row_height;

          attached = gtk_menu_get_attach_widget (menu);
          row_height = gb_tree_get_row_height (GB_TREE (attached));

          *x = loc->x;
          *y = loc->y + row_height - req.height;
        }
    }
}

static void
check_visible_foreach (GtkWidget *widget,
                       gpointer   user_data)
{
  gboolean *at_least_one_visible = user_data;

  if (*at_least_one_visible == FALSE)
    *at_least_one_visible = gtk_widget_get_visible (widget);
}

static GMenu *
gb_tree_create_menu (GbTree     *self,
                     GbTreeNode *node)
{
  GMenu *menu;
  guint i;
  GbTreePrivate *priv = gb_tree_get_instance_private (self);

  g_return_val_if_fail (GB_IS_TREE (self), NULL);
  g_return_val_if_fail (GB_IS_TREE_NODE (node), NULL);

  menu = g_menu_new ();

  for (i = 0; i < priv->builders->len; i++)
    {
      GbTreeBuilder *builder;

      builder = g_ptr_array_index (priv->builders, i);
      gb_tree_builder_node_popup (builder, node, menu);
    }

  return menu;
}

static void
gb_tree_popup (GbTree         *tree,
               GbTreeNode     *node,
               GdkEventButton *event,
               gint            target_x,
               gint            target_y)
{
  GdkPoint loc = { -1, -1 };
  gboolean at_least_one_visible = FALSE;
  GtkWidget *menu_widget;
  GMenu *menu;
  gint button;
  gint event_time;

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  menu = gb_tree_create_menu (tree, node);
  menu_widget = gtk_menu_new_from_model (G_MENU_MODEL (menu));
  g_clear_object (&menu);

  g_signal_emit (tree, gSignals [POPULATE_POPUP], 0, menu_widget);

  if ((target_x >= 0) && (target_y >= 0))
    {
      gdk_window_get_root_coords (gtk_widget_get_window (GTK_WIDGET (tree)),
                                  target_x, target_y, &loc.x, &loc.y);
      loc.x -= 12;
    }

  gtk_container_foreach (GTK_CONTAINER (menu_widget),
                         check_visible_foreach,
                         &at_least_one_visible);

  if (event != NULL)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  if (at_least_one_visible)
    {
      gtk_menu_attach_to_widget (GTK_MENU (menu_widget),
                                 GTK_WIDGET (tree),
                                 NULL);
      gtk_menu_popup (GTK_MENU (menu_widget), NULL, NULL,
                      gb_tree_menu_position_func, &loc,
                      button, event_time);
    }

  IDE_EXIT;
}

static gboolean
gb_tree_popup_menu (GtkWidget *widget)
{
  GbTree *tree = (GbTree *)widget;
  GbTreeNode *node;
  GdkRectangle area;

  g_assert (GB_IS_TREE (tree));

  if (!(node = gb_tree_get_selected (tree)))
    return FALSE;

  gb_tree_node_get_area (node, &area);
  gb_tree_popup (tree, node, NULL, area.x + area.width, area.y - 1);

  return TRUE;
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
  GbTreeBuilder *builder;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *node;
  GbTreeNode *unselection;
  gint i;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

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

  g_object_notify_by_pspec (G_OBJECT (tree), gParamSpecs [PROP_SELECTION]);

  IDE_EXIT;
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
  GtkTreeModel *model;
  GbTreeNode *that = NULL;
  gboolean ret;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_val_if_fail (GB_IS_TREE (tree), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GB_IS_TREE_NODE (node), FALSE);

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
              return TRUE;
            }
          g_clear_object (&that);
        }
      while (gtk_tree_model_iter_next (model, iter));
    }

  return FALSE;
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
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter old_iter;
  GtkTreeIter *parent = NULL;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

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

  IDE_ENTRY;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_tree_model_get (model, iter, 0, &node, -1);
  gb_tree_builder_build_node (builder, node);
  g_clear_object (&node);

  IDE_RETURN (FALSE);
}

gboolean
gb_tree_foreach (GbTree                  *tree,
                 GtkTreeIter             *iter,
                 GtkTreeModelForeachFunc  func,
                 gpointer                 user_data)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter child;
  gboolean ret;

  g_assert (GB_IS_TREE (tree));
  g_assert (iter != NULL);
  g_assert (gtk_tree_store_iter_is_valid (priv->store, iter));
  g_assert (func != NULL);

  model = GTK_TREE_MODEL (priv->store);
  path = gtk_tree_model_get_path (model, iter);
  ret = func (model, path, iter, user_data);
  gtk_tree_path_free (path);

  if (ret)
    return TRUE;

  if (gtk_tree_model_iter_children (model, &child, iter))
    {
      do
        {
          if (gb_tree_foreach (tree, &child, func, user_data))
            return TRUE;
        }
      while (gtk_tree_model_iter_next (model, &child));
    }

  return FALSE;
}

void
_gb_tree_rebuild_node (GbTree     *tree,
                       GbTreeNode *node)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter child;
  guint i;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  model = GTK_TREE_MODEL (priv->store);
  path = gb_tree_node_get_path (node);
  gtk_tree_model_get_iter (model, &iter, path);

  if (gtk_tree_model_iter_children (model, &child, &iter))
    {
      while (gtk_tree_store_remove (priv->store, &child))
        { /* Do Nothing */ }
    }

  priv->building++;
  for (i = 0; i < priv->builders->len; i++)
    {
      GbTreeBuilder *builder;

      /*
       * FIXME:
       *
       * Refactor this to do all builders when walking each node.
       */

      builder = g_ptr_array_index (priv->builders, i);
      gb_tree_foreach (tree,
                       &iter,
                       gb_tree_add_builder_foreach_cb,
                       builder);
      priv->building--;
    }

  gtk_tree_path_free (path);
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
  GtkTreeIter iter;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));

  g_object_set (builder, "tree", tree, NULL);
  g_ptr_array_add (priv->builders, g_object_ref_sink (builder));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
    {
      priv->building++;
      gb_tree_foreach (tree, &iter, gb_tree_add_builder_foreach_cb, builder);
      priv->building--;
    }

  if (GB_TREE_BUILDER_GET_CLASS (builder)->added)
    GB_TREE_BUILDER_GET_CLASS (builder)->added (builder, GTK_WIDGET (tree));

  IDE_EXIT;
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
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_BUILDER (builder));

  if (GB_TREE_BUILDER_GET_CLASS (builder)->removed)
    GB_TREE_BUILDER_GET_CLASS (builder)->removed (builder, GTK_WIDGET (tree));

  g_ptr_array_remove (priv->builders, builder);

  IDE_EXIT;
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
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_val_if_fail (GB_IS_TREE (tree), NULL);

  return priv->root;
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
  GbTreeBuilder *builder;
  gint i;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  IDE_ENTRY;

  g_return_if_fail (GB_IS_TREE (tree));

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

  IDE_EXIT;
}

void
gb_tree_rebuild (GbTree *tree)
{
  GbTreeNode *root;
  GtkTreeSelection *selection;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_if_fail (GB_IS_TREE (tree));

  /* avoid dealign with selection changes while rebuilding */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  gtk_tree_selection_unselect_all (selection);

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
  GbTreeBuilder *builder;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter that;
  gint i;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (GB_IS_TREE_NODE (child));

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
  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (GB_IS_TREE_NODE (child));

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
  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));
  g_return_if_fail (GB_IS_TREE_NODE (child));

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
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *node = NULL;
  gboolean handled = FALSE;
  GbTree *tree = (GbTree *) tree_view;
  gint i;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (path != NULL);

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
  GtkTreePath *tree_path = NULL;
  GtkTreeIter iter;
  GbTreeNode *node = NULL;
  gint cell_y;
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_return_val_if_fail (GB_IS_TREE (tree), FALSE);

  if ((button->type == GDK_BUTTON_PRESS) &&
      (button->button == GDK_BUTTON_SECONDARY))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (tree)))
        gtk_widget_grab_focus (GTK_WIDGET (tree));

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

void
gb_tree_scroll_to_node (GbTree     *tree,
                        GbTreeNode *node)
{
  GtkTreePath *path;

  g_return_if_fail (GB_IS_TREE (tree));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  path = gb_tree_node_get_path (node);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree), path, NULL, FALSE, 0, 0);
  gtk_tree_path_free (path);
}

static gboolean
gb_tree_find_item_foreach_cb (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      data)
{
  GbTreeNode *node = NULL;
  NodeLookup *lookup = data;

  gtk_tree_model_get (model, iter, 0, &node, -1);

  if (node != NULL)
    {
      GObject *item;

      item = gb_tree_node_get_item (node);

      if (lookup->equal_func (lookup->key, item))
        {
          lookup->result = node;
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gb_tree_find_custom:
 * @self: A #GbTree
 * @equal_func: A #GEqualFunc
 * @key: the key for @equal_func
 *
 * Walks the entire tree looking for the first item that matches given
 * @equal_func and @key.
 *
 * The first parameter to @equal_func will always be @key.
 * The second parameter will be the nodes #GbTreeNode:item property.
 *
 * Returns: (nullable) (transfer none): A #GbTreeNode or %NULL.
 */
GbTreeNode *
gb_tree_find_custom (GbTree     *self,
                     GEqualFunc  equal_func,
                     gpointer    key)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (self);
  NodeLookup lookup;

  g_return_val_if_fail (GB_IS_TREE (self), NULL);
  g_return_val_if_fail (equal_func != NULL, NULL);

  lookup.key = key;
  lookup.equal_func = equal_func;
  lookup.result = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                          gb_tree_find_item_foreach_cb,
                          &lookup);

  return lookup.result;
}

GbTreeNode *
gb_tree_find_item (GbTree  *self,
                   GObject *item)
{
  GbTreePrivate *priv = gb_tree_get_instance_private (self);
  NodeLookup lookup;

  g_return_val_if_fail (GB_IS_TREE (self), NULL);
  g_return_val_if_fail (!item || G_IS_OBJECT (item), NULL);

  lookup.key = item;
  lookup.equal_func = g_direct_equal;
  lookup.result = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                          gb_tree_find_item_foreach_cb,
                          &lookup);

  return lookup.result;
}

static void
gb_tree_real_action (GbTree      *self,
                     const gchar *prefix,
                     const gchar *action_name,
                     const gchar *param)
{
  GVariant *variant = NULL;

  g_assert (GB_IS_TREE (self));

  if (*param != 0)
    {
      GError *error = NULL;

      variant = g_variant_parse (NULL, param, NULL, NULL, &error);

      if (variant == NULL)
        {
          g_warning ("can't parse keybinding parameters \"%s\": %s",
                     param, error->message);
          g_clear_error (&error);
          return;
        }
    }

  gb_widget_activate_action (GTK_WIDGET (self), prefix, action_name, variant);
}

static gboolean
gb_tree_default_search_equal_func (GtkTreeModel *model,
                                   gint          column,
                                   const gchar  *key,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  GbTreeNode *node = NULL;
  gboolean ret = TRUE;

  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (column == 0);
  g_assert (key != NULL);
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &node, -1);

  if (node != NULL)
    {
      const gchar *text;

      text = gb_tree_node_get_text (node);
      ret = !(strstr (key, text) != NULL);
      g_object_unref (node);
    }

  return ret;
}

static void
gb_tree_add_child (GtkBuildable *buildable,
                   GtkBuilder   *builder,
                   GObject      *child,
                   const gchar  *type)
{
  GbTree *self = (GbTree *)buildable;

  g_assert (GB_IS_TREE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (g_strcmp0 (type, "builder") == 0)
    {
      if (!GB_IS_TREE_BUILDER (child))
        {
          g_warning ("Attempt to add invalid builder of type %s to GbTree.",
                     g_type_name (G_OBJECT_TYPE (child)));
          return;
        }

      gb_tree_add_builder (self, GB_TREE_BUILDER (child));
      return;
    }

  gb_tree_parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gb_tree_finalize (GObject *object)
{
  GbTree *tree = GB_TREE (object);
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  g_ptr_array_unref (priv->builders);
  g_clear_object (&priv->store);
  g_clear_object (&priv->root);

  G_OBJECT_CLASS (gb_tree_parent_class)->finalize (object);
}

static void
gb_tree_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GbTree *tree = GB_TREE (object);
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_object (value, priv->root);
      break;

    case PROP_SELECTION:
      g_value_set_object (value, priv->selection);
      break;

    case PROP_SHOW_ICONS:
      g_value_set_boolean (value, priv->show_icons);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

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

    case PROP_SHOW_ICONS:
      gb_tree_set_show_icons (tree, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tree_buildable_init (GtkBuildableIface *iface)
{
  gb_tree_parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gb_tree_add_child;
}

static void
gb_tree_class_init (GbTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_tree_finalize;
  object_class->get_property = gb_tree_get_property;
  object_class->set_property = gb_tree_set_property;

  widget_class->popup_menu = gb_tree_popup_menu;

  klass->action = gb_tree_real_action;

  gParamSpecs[PROP_ROOT] =
    g_param_spec_object ("root",
                         _ ("Root"),
                         _ ("The root object of the tree."),
                         GB_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  gParamSpecs[PROP_SELECTION] =
    g_param_spec_object ("selection",
                         _ ("Selection"),
                         _ ("The node selection."),
                         GB_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  gParamSpecs [PROP_SHOW_ICONS] =
    g_param_spec_boolean ("show-icons",
                          _("Show Icons"),
                          _("Show Icons"),
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [ACTION] =
    g_signal_new ("action",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbTreeClass, action),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  gSignals [POPULATE_POPUP] =
    g_signal_new ("populate-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTreeClass, populate_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);
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
  GbTreePrivate *priv = gb_tree_get_instance_private (tree);

  priv->builders = g_ptr_array_new ();
  g_ptr_array_set_free_func (priv->builders, g_object_unref);
  priv->store = gtk_tree_store_new (1, GB_TYPE_TREE_NODE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (gb_tree_selection_changed),
                            tree);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "title", "Node",
                         NULL);
  priv->column = GTK_TREE_VIEW_COLUMN (column);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "xpad", 3,
                       "visible", priv->show_icons,
                       NULL);
  priv->cell_pixbuf = cell;
  g_object_bind_property (tree, "show-icons", cell, "visible", 0);
  gtk_cell_layout_pack_start (column, cell, FALSE);
  gtk_cell_layout_set_cell_data_func (column, cell, pixbuf_func, NULL, NULL);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_NONE,
                       NULL);
  priv->cell_text = cell;
  gtk_cell_layout_pack_start (column, cell, TRUE);
  gtk_cell_layout_set_cell_data_func (column, cell, text_func, NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree),
                               GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree),
                           GTK_TREE_MODEL (priv->store));

  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (tree),
                                       gb_tree_default_search_equal_func,
                                       NULL, NULL);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (tree), 0);

  g_signal_connect (tree, "row-activated",
                    G_CALLBACK (gb_tree_row_activated),
                    NULL);
  g_signal_connect (tree, "button-press-event",
                    G_CALLBACK (gb_tree_button_press_event),
                    NULL);
}

void
gb_tree_expand_to_node (GbTree     *tree,
                        GbTreeNode *node)
{
  g_assert (GB_IS_TREE (tree));
  g_assert (GB_IS_TREE_NODE (node));

  if (gb_tree_node_get_expanded (node))
    {
      gb_tree_node_expand (node, TRUE);
    }
  else
    {
      gb_tree_node_expand (node, TRUE);
      gb_tree_node_collapse (node);
    }
}
