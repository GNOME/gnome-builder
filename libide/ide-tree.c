/* ide-tree.c
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

#include "ide-gtk.h"
#include "ide-tree.h"
#include "ide-tree-node.h"
#include "ide-tree-private.h"

typedef struct
{
  GPtrArray         *builders;
  IdeTreeNode        *root;
  IdeTreeNode        *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *cell_pixbuf;
  GtkCellRenderer   *cell_text;
  GtkTreeStore      *store;
  GdkRGBA            dim_foreground;
  guint              show_icons : 1;
} IdeTreePrivate;

typedef struct
{
  gpointer    key;
  GEqualFunc  equal_func;
  IdeTreeNode *result;
} NodeLookup;

typedef struct
{
  IdeTree           *self;
  IdeTreeFilterFunc  filter_func;
  gpointer          filter_data;
  GDestroyNotify    filter_data_destroy;
} FilterFunc;

static void ide_tree_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeTree, ide_tree, GTK_TYPE_TREE_VIEW,
                         G_ADD_PRIVATE (IdeTree)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, ide_tree_buildable_init))

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

static GtkBuildableIface *ide_tree_parent_buildable_iface;
static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

void
_ide_tree_build_node (IdeTree     *self,
                     IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  gsize i;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  _ide_tree_node_set_needs_build (node, FALSE);
  _ide_tree_node_remove_dummy_child (node);

  for (i = 0; i < priv->builders->len; i++)
    {
      IdeTreeBuilder *builder;

      builder = g_ptr_array_index (priv->builders, i);
      _ide_tree_builder_build_node (builder, node);
    }
}

static void
ide_tree_unselect (IdeTree *self)
{
  GtkTreeSelection *selection;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  gtk_tree_selection_unselect_all (selection);

  IDE_EXIT;
}

static void
ide_tree_select (IdeTree     *self,
                IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeSelection *selection;
  GtkTreePath *path;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  if (priv->selection)
    {
      ide_tree_unselect (self);
      g_assert (!priv->selection);
    }

  priv->selection = node;

  path = ide_tree_node_get_path (node);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  IDE_EXIT;
}

static guint
ide_tree_get_row_height (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  guint extra_padding;
  gint pix_min_height;
  gint pix_nat_height;
  gint text_min_height;
  gint text_nat_height;

  g_assert (IDE_IS_TREE (self));

  gtk_widget_style_get (GTK_WIDGET (self),
                        "vertical-separator", &extra_padding,
                        NULL);

  gtk_cell_renderer_get_preferred_height (priv->cell_pixbuf,
                                          GTK_WIDGET (self),
                                          &pix_min_height,
                                          &pix_nat_height);
  gtk_cell_renderer_get_preferred_height (priv->cell_text,
                                          GTK_WIDGET (self),
                                          &text_min_height,
                                          &text_nat_height);

  return MAX (pix_nat_height, text_nat_height) + extra_padding;
}

static void
ide_tree_menu_position_func (GtkMenu  *menu,
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
          row_height = ide_tree_get_row_height (IDE_TREE (attached));

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
ide_tree_create_menu (IdeTree     *self,
                     IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GMenu *menu;
  guint i;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  menu = g_menu_new ();

  for (i = 0; i < priv->builders->len; i++)
    {
      IdeTreeBuilder *builder;

      builder = g_ptr_array_index (priv->builders, i);
      _ide_tree_builder_node_popup (builder, node, menu);
    }

  return menu;
}

static void
ide_tree_popup (IdeTree         *self,
               IdeTreeNode     *node,
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

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  menu = ide_tree_create_menu (self, node);
  menu_widget = gtk_menu_new_from_model (G_MENU_MODEL (menu));
  g_clear_object (&menu);

  g_signal_emit (self, signals [POPULATE_POPUP], 0, menu_widget);

  if ((target_x >= 0) && (target_y >= 0))
    {
      gdk_window_get_root_coords (gtk_widget_get_window (GTK_WIDGET (self)),
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
                                 GTK_WIDGET (self),
                                 NULL);
      gtk_menu_popup (GTK_MENU (menu_widget), NULL, NULL,
                      ide_tree_menu_position_func, &loc,
                      button, event_time);
    }

  IDE_EXIT;
}

static gboolean
ide_tree_popup_menu (GtkWidget *widget)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreeNode *node;
  GdkRectangle area;

  g_assert (IDE_IS_TREE (self));

  if (!(node = ide_tree_get_selected (self)))
    return FALSE;

  ide_tree_node_get_area (node, &area);
  ide_tree_popup (self, node, NULL, area.x + area.width, area.y - 1);

  return TRUE;
}

static void
ide_tree_selection_changed (IdeTree           *self,
                           GtkTreeSelection *selection)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  IdeTreeBuilder *builder;
  GtkTreeModel *model;
  GtkTreeIter iter;
  IdeTreeNode *node;
  IdeTreeNode *unselection;
  gint i;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  if ((unselection = priv->selection))
    {
      priv->selection = NULL;
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          _ide_tree_builder_node_unselected (builder, unselection);
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
              _ide_tree_builder_node_selected (builder, node);
            }
          g_object_unref (node);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION]);

  IDE_EXIT;
}

static gboolean
ide_tree_add_builder_foreach_cb (GtkTreeModel *model,
                                GtkTreePath  *path,
                                GtkTreeIter  *iter,
                                gpointer      user_data)
{
  IdeTreeBuilder *builder = user_data;
  IdeTreeNode *node = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  gtk_tree_model_get (model, iter, 0, &node, -1);
  if (!_ide_tree_node_get_needs_build (node))
    _ide_tree_builder_build_node (builder, node);
  g_clear_object (&node);

  IDE_RETURN (FALSE);
}

static gboolean
ide_tree_foreach (IdeTree                  *self,
                 GtkTreeIter             *iter,
                 GtkTreeModelForeachFunc  func,
                 gpointer                 user_data)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter child;
  gboolean ret;

  g_assert (IDE_IS_TREE (self));
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
          if (ide_tree_foreach (self, &child, func, user_data))
            return TRUE;
        }
      while (gtk_tree_model_iter_next (model, &child));
    }

  return FALSE;
}

static void
pixbuf_func (GtkCellLayout   *cell_layout,
             GtkCellRenderer *cell,
             GtkTreeModel    *tree_model,
             GtkTreeIter     *iter,
             gpointer         data)
{
  const gchar *icon_name;
  IdeTreeNode *node;

  g_assert (GTK_IS_CELL_LAYOUT (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_PIXBUF (cell));
  g_assert (GTK_IS_TREE_MODEL (tree_model));
  g_assert (iter != NULL);

  gtk_tree_model_get (tree_model, iter, 0, &node, -1);
  icon_name = node ? ide_tree_node_get_icon_name (node) : NULL;
  g_object_set (cell, "icon-name", icon_name, NULL);
  g_clear_object (&node);
}

static void
text_func (GtkCellLayout   *cell_layout,
           GtkCellRenderer *cell,
           GtkTreeModel    *tree_model,
           GtkTreeIter     *iter,
           gpointer         data)
{
  IdeTree *self = data;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  IdeTreeNode *node = NULL;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_CELL_LAYOUT (cell_layout));
  g_assert (GTK_IS_CELL_RENDERER_TEXT (cell));
  g_assert (GTK_IS_TREE_MODEL (tree_model));
  g_assert (iter != NULL);

  gtk_tree_model_get (tree_model, iter, 0, &node, -1);

  if (node)
    {
      GdkRGBA *rgba = NULL;
      const gchar *text;
      gboolean use_markup;

      text = ide_tree_node_get_text (node);
      use_markup = ide_tree_node_get_use_markup (node);

      if (ide_tree_node_get_use_dim_label (node))
        rgba = &priv->dim_foreground;

      g_object_set (cell,
                    use_markup ? "markup" : "text", text,
                    "foreground-rgba", rgba,
                    NULL);
    }
}

static void
ide_tree_add (IdeTree     *self,
             IdeTreeNode *node,
             IdeTreeNode *child,
             gboolean    prepend)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreePath *path;
  GtkTreeIter *parentptr = NULL;
  GtkTreeIter iter;
  GtkTreeIter parent;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));

  _ide_tree_node_set_tree (child, self);
  _ide_tree_node_set_parent (child, node);

  g_object_ref_sink (child);

  if (node != priv->root)
    {
      path = ide_tree_node_get_path (node);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &parent, path);
      parentptr = &parent;
      g_clear_pointer (&path, gtk_tree_path_free);
    }

  gtk_tree_store_insert_with_values (priv->store, &iter, parentptr,
                                     prepend ? 0 : -1,
                                     0, child,
                                     -1);

  if (node == priv->root)
    _ide_tree_build_node (self, child);

  g_object_unref (child);
}

void
_ide_tree_insert_sorted (IdeTree                *self,
                        IdeTreeNode            *node,
                        IdeTreeNode            *child,
                        IdeTreeNodeCompareFunc  compare_func,
                        gpointer               user_data)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreeIter *parent = NULL;
  GtkTreeIter node_iter;
  GtkTreeIter children;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (compare_func != NULL);

  model = GTK_TREE_MODEL (priv->store);

  _ide_tree_node_set_tree (child, self);
  _ide_tree_node_set_parent (child, node);

  g_object_ref_sink (child);

  if (ide_tree_node_get_iter (node, &node_iter))
    parent = &node_iter;

  if (gtk_tree_model_iter_children (model, &children, parent))
    {
      do
        {
          g_autoptr(IdeTreeNode) sibling = NULL;
          GtkTreeIter that;

          gtk_tree_model_get (model, &children, 0, &sibling, -1);

          if (compare_func (sibling, child, user_data) > 0)
            {
              gtk_tree_store_insert_before (priv->store, &that, parent, &children);
              gtk_tree_store_set (priv->store, &that, 0, child, -1);
              goto inserted;
            }
        }
      while (gtk_tree_model_iter_next (model, &children));
    }

  gtk_tree_store_append (priv->store, &children, parent);
  gtk_tree_store_set (priv->store, &children, 0, child, -1);

inserted:
  if (node == priv->root)
    _ide_tree_build_node (self, child);

  g_object_unref (child);
}

static void
ide_tree_row_activated (GtkTreeView       *tree_view,
                       GtkTreePath       *path,
                       GtkTreeViewColumn *column)
{
  IdeTree *self = (IdeTree *)tree_view;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  IdeTreeBuilder *builder;
  GtkTreeModel *model;
  GtkTreeIter iter;
  IdeTreeNode *node = NULL;
  gboolean handled = FALSE;
  gint i;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (path != NULL);

  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gtk_tree_model_get (model, &iter, 0, &node, -1);
      for (i = 0; i < priv->builders->len; i++)
        {
          builder = g_ptr_array_index (priv->builders, i);
          if ((handled = _ide_tree_builder_node_activated (builder, node)))
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

static void
ide_tree_row_expanded (GtkTreeView *tree_view,
                      GtkTreeIter *iter,
                      GtkTreePath *path)
{
  IdeTree *self = (IdeTree *)tree_view;
  GtkTreeModel *model;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE (self));
  g_assert (iter != NULL);
  g_assert (path != NULL);

  model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get (model, iter, 0, &node, -1);

  /*
   * If we are expanding a row that has a dummy child, we might need to
   * build the node immediately, and re-expand it.
   */
  if (_ide_tree_node_get_needs_build (node))
    {
      _ide_tree_build_node (self, node);
      ide_tree_node_expand (node, FALSE);
      ide_tree_node_select (node);
    }

  g_clear_object (&node);
}


static gboolean
ide_tree_button_press_event (GtkWidget      *widget,
                            GdkEventButton *button)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkAllocation alloc;
  GtkTreePath *tree_path = NULL;
  GtkTreeIter iter;
  IdeTreeNode *node = NULL;
  gint cell_y;

  g_assert (IDE_IS_TREE (self));
  g_assert (button != NULL);

  if ((button->type == GDK_BUTTON_PRESS) && (button->button == GDK_BUTTON_SECONDARY))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self)))
        gtk_widget_grab_focus (GTK_WIDGET (self));

      gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (self),
                                     button->x,
                                     button->y,
                                     &tree_path,
                                     NULL,
                                     NULL,
                                     &cell_y);

      if (!tree_path)
        {
          ide_tree_unselect (self);
        }
      else
        {
          gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
          gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, tree_path);
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, 0, &node, -1);
          ide_tree_select (self, node);
          ide_tree_popup (self, node, button,
                         alloc.x + alloc.width,
                         button->y - cell_y);
          g_object_unref (node);
          gtk_tree_path_free (tree_path);
        }

      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (ide_tree_parent_class)->button_press_event (widget, button);
}

static gboolean
ide_tree_find_item_foreach_cb (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      user_data)
{
  IdeTreeNode *node = NULL;
  NodeLookup *lookup = user_data;
  gboolean ret = FALSE;

  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (path != NULL);
  g_assert (iter != NULL);
  g_assert (lookup != NULL);

  gtk_tree_model_get (model, iter, 0, &node, -1);

  if (node != NULL)
    {
      GObject *item;

      item = ide_tree_node_get_item (node);

      if (lookup->equal_func (lookup->key, item))
        {
          lookup->result = node;
          ret = TRUE;
        }
    }

  g_clear_object (&node);

  return ret;
}

static void
ide_tree_real_action (IdeTree      *self,
                     const gchar *prefix,
                     const gchar *action_name,
                     const gchar *param)
{
  GVariant *variant = NULL;

  g_assert (IDE_IS_TREE (self));

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

  ide_widget_action (GTK_WIDGET (self), prefix, action_name, variant);
}

static gboolean
ide_tree_default_search_equal_func (GtkTreeModel *model,
                                   gint          column,
                                   const gchar  *key,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  IdeTreeNode *node = NULL;
  gboolean ret = TRUE;

  g_assert (GTK_IS_TREE_MODEL (model));
  g_assert (column == 0);
  g_assert (key != NULL);
  g_assert (iter != NULL);

  gtk_tree_model_get (model, iter, 0, &node, -1);

  if (node != NULL)
    {
      const gchar *text;

      text = ide_tree_node_get_text (node);
      ret = !(strstr (key, text) != NULL);
      g_object_unref (node);
    }

  return ret;
}

static void
ide_tree_add_child (GtkBuildable *buildable,
                   GtkBuilder   *builder,
                   GObject      *child,
                   const gchar  *type)
{
  IdeTree *self = (IdeTree *)buildable;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (g_strcmp0 (type, "builder") == 0)
    {
      if (!IDE_IS_TREE_BUILDER (child))
        {
          g_warning ("Attempt to add invalid builder of type %s to IdeTree.",
                     g_type_name (G_OBJECT_TYPE (child)));
          return;
        }

      ide_tree_add_builder (self, IDE_TREE_BUILDER (child));
      return;
    }

  ide_tree_parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
ide_tree_style_updated (GtkWidget *widget)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkStyleContext *style_context;
  GtkStateFlags flags;

  g_assert (IDE_IS_TREE (self));

  GTK_WIDGET_CLASS (ide_tree_parent_class)->style_updated (widget);

  flags = gtk_widget_get_state_flags (widget);

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, "dim-label");
  gtk_style_context_get_color (style_context, flags, &priv->dim_foreground);
  gtk_style_context_restore (style_context);
}

static void
ide_tree_finalize (GObject *object)
{
  IdeTree *self = IDE_TREE (object);
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_ptr_array_unref (priv->builders);
  g_clear_object (&priv->store);
  g_clear_object (&priv->root);

  G_OBJECT_CLASS (ide_tree_parent_class)->finalize (object);
}

static void
ide_tree_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  IdeTree *self = IDE_TREE (object);
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

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
ide_tree_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  IdeTree *self = IDE_TREE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      ide_tree_set_root (self, g_value_get_object (value));
      break;

    case PROP_SELECTION:
      ide_tree_select (self, g_value_get_object (value));
      break;

    case PROP_SHOW_ICONS:
      ide_tree_set_show_icons (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_buildable_init (GtkBuildableIface *iface)
{
  ide_tree_parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = ide_tree_add_child;
}

static void
ide_tree_class_init (IdeTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkTreeViewClass *tree_view_class = GTK_TREE_VIEW_CLASS (klass);

  object_class->finalize = ide_tree_finalize;
  object_class->get_property = ide_tree_get_property;
  object_class->set_property = ide_tree_set_property;

  widget_class->popup_menu = ide_tree_popup_menu;
  widget_class->button_press_event = ide_tree_button_press_event;
  widget_class->style_updated = ide_tree_style_updated;

  tree_view_class->row_activated = ide_tree_row_activated;
  tree_view_class->row_expanded = ide_tree_row_expanded;

  klass->action = ide_tree_real_action;

  properties[PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root object of the tree.",
                         IDE_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTION] =
    g_param_spec_object ("selection",
                         "Selection",
                         "The node selection.",
                         IDE_TYPE_TREE_NODE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_ICONS] =
    g_param_spec_boolean ("show-icons",
                          "Show Icons",
                          "Show Icons",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ACTION] =
    g_signal_new ("action",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeTreeClass, action),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_STRING);

  signals [POPULATE_POPUP] =
    g_signal_new ("populate-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTreeClass, populate_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);
}

static void
ide_tree_init (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkCellLayout *column;

  priv->builders = g_ptr_array_new ();
  g_ptr_array_set_free_func (priv->builders, g_object_unref);
  priv->store = gtk_tree_store_new (1, IDE_TYPE_TREE_NODE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  g_signal_connect_object (selection, "changed",
                           G_CALLBACK (ide_tree_selection_changed),
                           self,
                           G_CONNECT_SWAPPED);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "title", "Node",
                         NULL);
  priv->column = GTK_TREE_VIEW_COLUMN (column);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "xpad", 3,
                       "visible", priv->show_icons,
                       NULL);
  priv->cell_pixbuf = cell;
  g_object_bind_property (self, "show-icons", cell, "visible", 0);
  gtk_cell_layout_pack_start (column, cell, FALSE);
  gtk_cell_layout_set_cell_data_func (column, cell, pixbuf_func, NULL, NULL);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_NONE,
                       NULL);
  priv->cell_text = cell;
  gtk_cell_layout_pack_start (column, cell, TRUE);
  gtk_cell_layout_set_cell_data_func (column, cell, text_func, self, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (self),
                               GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_view_set_model (GTK_TREE_VIEW (self),
                           GTK_TREE_MODEL (priv->store));

  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self),
                                       ide_tree_default_search_equal_func,
                                       NULL, NULL);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (self), 0);
}

void
ide_tree_expand_to_node (IdeTree     *self,
                        IdeTreeNode *node)
{
  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  if (ide_tree_node_get_expanded (node))
    {
      ide_tree_node_expand (node, TRUE);
    }
  else
    {
      ide_tree_node_expand (node, TRUE);
      ide_tree_node_collapse (node);
    }
}

gboolean
ide_tree_get_show_icons (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), FALSE);

  return priv->show_icons;
}

void
ide_tree_set_show_icons (IdeTree   *self,
                        gboolean  show_icons)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_if_fail (IDE_IS_TREE (self));

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
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties [PROP_SHOW_ICONS]);
    }
}

/**
 * ide_tree_get_selected:
 * @self: (in): A #IdeTree.
 *
 * Gets the currently selected node in the tree.
 *
 * Returns: (transfer none): A #IdeTreeNode.
 */
IdeTreeNode *
ide_tree_get_selected (IdeTree *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  IdeTreeNode *ret = NULL;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
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

void
ide_tree_scroll_to_node (IdeTree     *self,
                        IdeTreeNode *node)
{
  GtkTreePath *path;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  path = ide_tree_node_get_path (node);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (self), path, NULL, FALSE, 0, 0);
  gtk_tree_path_free (path);
}

GtkTreePath *
_ide_tree_get_path (IdeTree *self,
                   GList  *list)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter *iter_ptr;
  GList *list_iter;

  g_assert (IDE_IS_TREE (self));

  model = GTK_TREE_MODEL (priv->store);

  if ((list == NULL) || (list->data != priv->root) || (list->next == NULL))
    return NULL;

  iter_ptr = NULL;

  for (list_iter = list->next; list_iter; list_iter = list_iter->next)
    {
      GtkTreeIter children;

      if (gtk_tree_model_iter_children (model, &children, iter_ptr))
        {
          gboolean found = FALSE;

          do
            {
              g_autoptr(IdeTreeNode) item = NULL;

              gtk_tree_model_get (model, &children, 0, &item, -1);
              found = (item == (IdeTreeNode *)list_iter->data);
            }
          while (!found && gtk_tree_model_iter_next (model, &children));

          if (found)
            {
              iter = children;
              iter_ptr = &iter;
              continue;
            }
        }

      return NULL;
    }

  return gtk_tree_model_get_path (model, &iter);
}

/**
 * ide_tree_add_builder:
 * @self: (in): A #IdeTree.
 * @builder: (in) (transfer full): A #IdeTreeBuilder to add.
 *
 * Removes a builder from the tree.
 */
void
ide_tree_add_builder (IdeTree        *self,
                     IdeTreeBuilder *builder)
{
  GtkTreeIter iter;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_BUILDER (builder));

  g_ptr_array_add (priv->builders, g_object_ref_sink (builder));

  _ide_tree_builder_set_tree (builder, self);
  _ide_tree_builder_added (builder, self);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
    ide_tree_foreach (self, &iter, ide_tree_add_builder_foreach_cb, builder);

  IDE_EXIT;
}

/**
 * ide_tree_remove_builder:
 * @self: (in): A #IdeTree.
 * @builder: (in): A #IdeTreeBuilder to remove.
 *
 * Removes a builder from the tree.
 */
void
ide_tree_remove_builder (IdeTree        *self,
                        IdeTreeBuilder *builder)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  gsize i;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_BUILDER (builder));

  for (i = 0; i < priv->builders->len; i++)
    {
      if (builder == g_ptr_array_index (priv->builders, i))
        {
          g_object_ref (builder);
          g_ptr_array_remove_index (priv->builders, i);
          _ide_tree_builder_removed (builder, self);
          g_object_unref (builder);
        }
    }

  IDE_EXIT;
}

/**
 * ide_tree_get_root:
 *
 * Retrieves the root node of the tree. The root node is not a visible node
 * in the self, but a placeholder for all other builders to build upon.
 *
 * Returns: (transfer none) (nullable): A #IdeTreeNode or %NULL.
 */
IdeTreeNode *
ide_tree_get_root (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  return priv->root;
}

/**
 * ide_tree_set_root:
 * @self: (in): A #IdeTree.
 * @node: (in): A #IdeTreeNode.
 *
 * Sets the root node of the #IdeTree widget. This is used to build
 * the items within the treeview. The item itself will not be added
 * to the self, but the direct children will be.
 */
void
ide_tree_set_root (IdeTree     *self,
                  IdeTreeNode *root)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TREE (self));

  if (priv->root != root)
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
      GtkTreeModel *current;

      gtk_tree_selection_unselect_all (selection);

      if (priv->root != NULL)
        {
          _ide_tree_node_set_parent (priv->root, NULL);
          _ide_tree_node_set_tree (priv->root, NULL);
          gtk_tree_store_clear (priv->store);
          g_clear_object (&priv->root);
        }

      current = gtk_tree_view_get_model (GTK_TREE_VIEW (self));
      if (GTK_IS_TREE_MODEL_FILTER (current))
        gtk_tree_model_filter_clear_cache (GTK_TREE_MODEL_FILTER (current));

      if (root != NULL)
        {
          priv->root = g_object_ref_sink (root);
          _ide_tree_node_set_parent (priv->root, NULL);
          _ide_tree_node_set_tree (priv->root, self);
          _ide_tree_build_node (self, priv->root);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT]);
    }

  IDE_EXIT;
}

void
ide_tree_rebuild (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeSelection *selection;

  g_return_if_fail (IDE_IS_TREE (self));

  /*
   * We don't want notification of selection changes while rebuilding.
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  gtk_tree_selection_unselect_all (selection);

  if (priv->root != NULL)
    {
      gtk_tree_store_clear (priv->store);
      _ide_tree_build_node (self, priv->root);
    }
}

/**
 * ide_tree_find_custom:
 * @self: A #IdeTree
 * @equal_func: (scope call): A #GEqualFunc
 * @key: the key for @equal_func
 *
 * Walks the entire tree looking for the first item that matches given
 * @equal_func and @key.
 *
 * The first parameter to @equal_func will always be @key.
 * The second parameter will be the nodes #IdeTreeNode:item property.
 *
 * Returns: (nullable) (transfer none): A #IdeTreeNode or %NULL.
 */
IdeTreeNode *
ide_tree_find_custom (IdeTree     *self,
                     GEqualFunc  equal_func,
                     gpointer    key)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  NodeLookup lookup;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);
  g_return_val_if_fail (equal_func != NULL, NULL);

  lookup.key = key;
  lookup.equal_func = equal_func;
  lookup.result = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                          ide_tree_find_item_foreach_cb,
                          &lookup);

  return lookup.result;
}

/**
 * ide_tree_find_item:
 * @self: A #IdeTree.
 * @item: (allow-none): A #GObject or %NULL.
 *
 * Finds a #IdeTreeNode with an item property matching @item.
 *
 * Returns: (transfer none) (nullable): A #IdeTreeNode or %NULL.
 */
IdeTreeNode *
ide_tree_find_item (IdeTree  *self,
                   GObject *item)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  NodeLookup lookup;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);
  g_return_val_if_fail (!item || G_IS_OBJECT (item), NULL);

  lookup.key = item;
  lookup.equal_func = g_direct_equal;
  lookup.result = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                          ide_tree_find_item_foreach_cb,
                          &lookup);

  return lookup.result;
}

void
_ide_tree_append (IdeTree     *self,
                 IdeTreeNode *node,
                 IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));

  ide_tree_add (self, node, child, FALSE);
}

void
_ide_tree_prepend (IdeTree     *self,
                  IdeTreeNode *node,
                  IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (IDE_IS_TREE_NODE (child));

  ide_tree_add (self, node, child, TRUE);
}

void
_ide_tree_invalidate (IdeTree     *self,
                     IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreePath *path;
  IdeTreeNode *parent;
  GtkTreeIter iter;
  GtkTreeIter child;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  model = GTK_TREE_MODEL (priv->store);
  path = ide_tree_node_get_path (node);
  gtk_tree_model_get_iter (model, &iter, path);

  if (gtk_tree_model_iter_children (model, &child, &iter))
    {
      while (gtk_tree_store_remove (priv->store, &child))
        {
        }
    }

  _ide_tree_node_set_needs_build (node, TRUE);

  parent = ide_tree_node_get_parent (node);

  if ((parent == NULL) || ide_tree_node_get_expanded (parent))
    _ide_tree_build_node (self, node);

  gtk_tree_path_free (path);
}

/**
 * ide_tree_find_child_node:
 * @self: A #IdeTree
 * @node: A #IdeTreeNode
 * @find_func: (scope call): A callback to locate the child
 * @user_data: user data for @find_func
 *
 * Searches through the direct children of @node for a matching child.
 * @find_func should return %TRUE if the child matches, otherwise %FALSE.
 *
 * Returns: (transfer none) (nullable): A #IdeTreeNode or %NULL.
 */
IdeTreeNode *
ide_tree_find_child_node (IdeTree         *self,
                         IdeTreeNode     *node,
                         IdeTreeFindFunc  find_func,
                         gpointer        user_data)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter children;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);
  g_return_val_if_fail (!node || IDE_IS_TREE_NODE (node), NULL);
  g_return_val_if_fail (find_func, NULL);

  if (node == NULL)
    node = priv->root;

  if (node == NULL)
    {
      g_warning ("Cannot find node. No root node has been set on %s.",
                 g_type_name (G_OBJECT_TYPE (self)));
      return NULL;
    }

  if (_ide_tree_node_get_needs_build (node))
    _ide_tree_build_node (self, node);

  model = GTK_TREE_MODEL (priv->store);
  path = ide_tree_node_get_path (node);

  if (path != NULL)
    {
      if (!gtk_tree_model_get_iter (model, &iter, path))
        goto failure;

      if (!gtk_tree_model_iter_children (model, &children, &iter))
        goto failure;
    }
  else
    {
      if (!gtk_tree_model_iter_children (model, &children, NULL))
        goto failure;
    }

  do
    {
      IdeTreeNode *child = NULL;

      gtk_tree_model_get (model, &children, 0, &child, -1);

      if (find_func (self, node, child, user_data))
        {
          /*
           * We want to returned a borrowed reference to the child node.
           * It is safe to unref the child here before we return.
           */
          g_object_unref (child);
          return child;
        }

      g_clear_object (&child);
    }
  while (gtk_tree_model_iter_next (model, &children));

failure:
  g_clear_pointer (&path, gtk_tree_path_free);

  return NULL;
}

void
_ide_tree_remove (IdeTree     *self,
                 IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  path = ide_tree_node_get_path (node);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path))
    gtk_tree_store_remove (priv->store, &iter);

  gtk_tree_path_free (path);
}

gboolean
_ide_tree_get_iter (IdeTree      *self,
                   IdeTreeNode  *node,
                   GtkTreeIter *iter)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreePath *path;
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_TREE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);
  g_return_val_if_fail (iter, FALSE);

  if ((path = ide_tree_node_get_path (node)) != NULL)
    {
      ret = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), iter, path);
      gtk_tree_path_free (path);
    }

  return ret;
}

static void
filter_func_free (gpointer user_data)
{
  FilterFunc *data = user_data;

  if (data->filter_data_destroy)
    data->filter_data_destroy (data->filter_data);

  g_free (data);
}

static gboolean
ide_tree_model_filter_recursive (GtkTreeModel *model,
                                GtkTreeIter  *parent,
                                FilterFunc   *filter)
{
  GtkTreeIter child;

  if (gtk_tree_model_iter_children (model, &child, parent))
    {
      do
        {
          g_autoptr(IdeTreeNode) node = NULL;

          gtk_tree_model_get (model, &child, 0, &node, -1);

          if ((node != NULL) && !_ide_tree_node_get_needs_build (node))
            {
              if (filter->filter_func (filter->self, node, filter->filter_data))
                return TRUE;

              if (ide_tree_model_filter_recursive (model, &child, filter))
                return TRUE;
            }
        }
      while (gtk_tree_model_iter_next (model, &child));
    }

  return FALSE;
}

static gboolean
ide_tree_model_filter_visible_func (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   gpointer      data)
{
  IdeTreeNode *node = NULL;
  FilterFunc *filter = data;
  gboolean ret;

  g_assert (filter != NULL);
  g_assert (IDE_IS_TREE (filter->self));
  g_assert (filter->filter_func != NULL);

  /*
   * This is a rather complex situation.
   *
   * We might not match, but one of our children nodes might match.
   * Furthering the issue, the children might still need to be built.
   * For some cases, this could be really expensive (think file tree)
   * but for other things, it could be cheap. If you are going to use
   * a filter func for your tree, you probably should avoid being
   * too lazy and ensure the nodes are available.
   *
   * Therefore, we will only check available nodes, and ignore the
   * case where the children nodes need to be built.   *
   *
   * TODO: Another option would be to iteratively build the items after
   *       the initial filter.
   */

  gtk_tree_model_get (model, iter, 0, &node, -1);
  ret = filter->filter_func (filter->self, node, filter->filter_data);
  g_clear_object (&node);

  /*
   * Short circuit if we already matched.
   */
  if (ret)
    return TRUE;

  /*
   * If any of our children match, we should match.
   */
  if (ide_tree_model_filter_recursive (model, iter, filter))
    return TRUE;

  return FALSE;
}

/**
 * ide_tree_set_filter:
 * @self: A #IdeTree
 * @filter_func: (scope notified): A callback to determien visibility.
 * @filter_data: User data for @filter_func.
 * @filter_data_destroy: Destroy notify for @filter_data.
 *
 * Sets the filter function to be used to determine visability of a tree node.
 */
void
ide_tree_set_filter (IdeTree           *self,
                    IdeTreeFilterFunc  filter_func,
                    gpointer          filter_data,
                    GDestroyNotify    filter_data_destroy)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_if_fail (IDE_IS_TREE (self));

  if (filter_func == NULL)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (priv->store));
    }
  else
    {
      FilterFunc *data;
      GtkTreeModel *filter;

      data = g_new0 (FilterFunc, 1);
      data->self = self;
      data->filter_func = filter_func;
      data->filter_data = filter_data;
      data->filter_data_destroy = filter_data_destroy;

      filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->store), NULL);
      gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                              ide_tree_model_filter_visible_func,
                                              data,
                                              filter_func_free);
      gtk_tree_view_set_model (GTK_TREE_VIEW (self), GTK_TREE_MODEL (filter));
      g_clear_object (&filter);
    }
}

GtkTreeStore *
_ide_tree_get_store (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  return priv->store;
}
