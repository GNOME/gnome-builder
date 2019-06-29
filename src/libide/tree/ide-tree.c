/* ide-tree.c
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

#define G_LOG_DOMAIN "ide-tree"

#include "config.h"

#include <libpeas/peas.h>
#include <libide-core.h>
#include <libide-threading.h>

#include "ide-cell-renderer-status.h"
#include "ide-tree.h"
#include "ide-tree-model.h"
#include "ide-tree-node.h"
#include "ide-tree-private.h"

typedef struct
{
  /* This #GCancellable will be automatically cancelled when the widget is
   * destroyed. That is usefulf or async operations that you want to be
   * cleaned up as the workspace is destroyed or the widget in question
   * removed from the widget tree.
   */
  GCancellable *cancellable;

  /* To keep rendering of common styles fast, we share these PangoAttrList
   * so that we need not re-create them many times.
   */
  PangoAttrList *dim_label_attributes;
  PangoAttrList *header_attributes;

  /* The context menu to use for popups */
  GMenu *context_menu;

  /* Our context menu popover */
  GtkPopover *popover;

  /* Stashed drop information to propagate on drop */
  GdkDragAction drop_action;
  GtkTreePath *drop_path;
  GtkTreeViewDropPosition drop_pos;
} IdeTreePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTree, ide_tree, GTK_TYPE_TREE_VIEW)

static IdeTreeModel *
ide_tree_get_model (IdeTree *self)
{
  GtkTreeModel *model;

  g_assert (IDE_IS_TREE (self));

  if (!(model = gtk_tree_view_get_model (GTK_TREE_VIEW (self))) ||
      !IDE_IS_TREE_MODEL (model))
    return NULL;

  return IDE_TREE_MODEL (model);
}

static void
ide_tree_selection_changed_cb (IdeTree          *self,
                               GtkTreeSelection *selection)
{
  IdeTreeModel *model;
  GtkTreeIter iter;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_TREE_SELECTION (selection));

  if (!(model = ide_tree_get_model (self)))
    return;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    _ide_tree_model_selection_changed (model, &iter);
  else
    _ide_tree_model_selection_changed (model, NULL);
}

static void
ide_tree_unselect (IdeTree *self)
{
  g_assert (IDE_IS_TREE (self));

  gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (self)));
}

static void
ide_tree_select (IdeTree     *self,
                 IdeTreeNode *node)
{
  g_autoptr(GtkTreePath) path = NULL;
  GtkTreeSelection *selection;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  ide_tree_unselect (self);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  path = ide_tree_node_get_path (node);
  gtk_tree_selection_select_path (selection, path);
}

static void
state_cell_func (GtkCellLayout   *layout,
                 GtkCellRenderer *cell,
                 GtkTreeModel    *model,
                 GtkTreeIter     *iter,
                 gpointer         user_data)
{
  IdeTreeNodeFlags flags = 0;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE (user_data));
  g_assert (IDE_IS_TREE_MODEL (model));

  if ((node = ide_tree_model_get_node (IDE_TREE_MODEL (model), iter)))
    flags = ide_tree_node_get_flags (node);

  ide_cell_renderer_status_set_flags (IDE_CELL_RENDERER_STATUS (cell), flags);
}

static void
text_cell_func (GtkCellLayout   *layout,
                GtkCellRenderer *cell,
                GtkTreeModel    *model,
                GtkTreeIter     *iter,
                gpointer         user_data)
{
  IdeTree *self = user_data;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  const gchar *display_name = NULL;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_MODEL (model));

  g_object_set (cell,
                "attributes", NULL,
                "foreground-set", FALSE,
                NULL);

  if (!(node = ide_tree_model_get_node (IDE_TREE_MODEL (model), iter)))
    return;

  _ide_tree_model_cell_data_func (IDE_TREE_MODEL (model), iter, cell);

  /* If we're loading the node, avoid showing the "Loading..." text for 250
   * milliseconds, so that we don't flash the user with information they'll
   * never be able to read.
   */
  if (ide_tree_node_is_empty (node))
    {
      IdeTreeNode *parent = ide_tree_node_get_parent (node);
      gint64 started_loading_at;

      if (_ide_tree_node_get_loading (parent, &started_loading_at))
        {
          gint64 now = g_get_monotonic_time ();

          if ((now - started_loading_at) < (G_USEC_PER_SEC / 4L))
            goto set_props;
        }
    }

  if (ide_tree_node_get_has_error (node))
    {
      PangoAttrList *attrs = NULL;
      PangoAttrList *copy = NULL;

      g_object_get (cell,
                    "attributes", &attrs,
                    NULL);

      if (attrs != NULL)
        copy = pango_attr_list_copy (attrs);
      else
        copy = pango_attr_list_new ();

      pango_attr_list_insert (copy, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
      pango_attr_list_insert (copy, pango_attr_underline_color_new (65535, 0, 0));

      g_object_set (cell,
                    "attributes", copy,
                    NULL);

      g_clear_pointer (&attrs, pango_attr_list_unref);
      g_clear_pointer (&copy, pango_attr_list_unref);
    }

  if (ide_tree_node_get_is_header (node) ||
      (ide_tree_node_get_flags (node) & IDE_TREE_NODE_FLAGS_ADDED))
    g_object_set (cell, "attributes", priv->header_attributes, NULL);
  else if (ide_tree_node_is_empty (node) && !ide_tree_node_is_selected (node))
    g_object_set (cell, "attributes", priv->dim_label_attributes, NULL);

  display_name = ide_tree_node_get_display_name (node);

set_props:
  if (ide_tree_node_get_use_markup (node))
    g_object_set (cell, "markup", display_name, NULL);
  else
    g_object_set (cell, "text", display_name, NULL);
}

static void
pixbuf_cell_func (GtkCellLayout   *layout,
                  GtkCellRenderer *cell,
                  GtkTreeModel    *model,
                  GtkTreeIter     *iter,
                  gpointer         user_data)
{
  IdeTree *self = user_data;
  g_autoptr(GtkTreePath) path = NULL;
  g_autoptr(GIcon) emblems = NULL;
  IdeTreeNode *node;
  GIcon *icon;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_MODEL (model));

  if (!(node = ide_tree_model_get_node (IDE_TREE_MODEL (model), iter)))
    return;

  path = gtk_tree_model_get_path (model, iter);

  if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (self), path))
    icon = ide_tree_node_get_expanded_icon (node);
  else
    icon = ide_tree_node_get_icon (node);

  if (icon != NULL)
    emblems = _ide_tree_node_apply_emblems (node, icon);

  g_object_set (cell, "gicon", emblems, NULL);
}

static void
ide_tree_expand_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdeTreeModel *model = (IdeTreeModel *)object;
  g_autoptr(GtkTreePath) path = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdeTreeNode *node;
  IdeTree *self;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  node = ide_task_get_task_data (task);

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  if ((path = ide_tree_model_get_path_for_node (model, node)))
    {
      if (ide_tree_model_expand_finish (model, result, NULL))
        {
          /* If node was detached during our async operation, we'll get NULL
           * back for the GtkTreePath (in which case, we'll just ignore).
           */
          gtk_tree_view_expand_row (GTK_TREE_VIEW (self), path, FALSE);
        }

      _ide_tree_model_row_expanded (model, self, path);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
ide_tree_row_activated (GtkTreeView       *tree_view,
                        GtkTreePath       *path,
                        GtkTreeViewColumn *column)
{
  IdeTree *self = (IdeTree *)tree_view;
  IdeTreeModel *model;
  IdeTreeNode *node;
  GtkTreeIter iter;

  g_assert (IDE_IS_TREE (self));
  g_assert (path != NULL);
  g_assert (GTK_IS_TREE_VIEW_COLUMN (column));

  /* Get our model, and the node in question. Ignore everything if this
   * is a synthesized "Empty" node.
   */
  if (!(model = ide_tree_get_model (self)) ||
      !gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path) ||
      !(node = ide_tree_model_get_node (model, &iter)) ||
      ide_tree_node_is_empty (node))
    return;

  if (!_ide_tree_model_row_activated (model, self, path))
    {
      if (gtk_tree_view_row_expanded (tree_view, path))
        gtk_tree_view_collapse_row (tree_view, path);
      else
        gtk_tree_view_expand_row (tree_view, path, FALSE);
    }
}

static void
ide_tree_row_expanded (GtkTreeView *tree_view,
                       GtkTreeIter *iter,
                       GtkTreePath *path)
{
  IdeTree *self = (IdeTree *)tree_view;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  IdeTreeModel *model;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE (tree_view));
  g_assert (iter != NULL);
  g_assert (IDE_IS_TREE_NODE (iter->user_data));
  g_assert (path != NULL);

  if (!(model = ide_tree_get_model (self)) ||
      !(node = ide_tree_model_get_node (model, iter)) ||
      !ide_tree_node_get_children_possible (node))
    return;

  task = ide_task_new (self, priv->cancellable, NULL, NULL);
  ide_task_set_source_tag (task, ide_tree_row_expanded);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  /* We want to expand the row if we can, but we need to ensure the
   * children have been built first (it might only have a fake "empty"
   * node currently). So we request that the model expand the row and
   * then expand to the path on the callback. The model will do nothing
   * more than complete the async request if there is nothing to build.
   */
  ide_tree_model_expand_async (IDE_TREE_MODEL (model),
                               node,
                               priv->cancellable,
                               ide_tree_expand_cb,
                               g_steal_pointer (&task));
}

static void
ide_tree_row_collapsed (GtkTreeView *tree_view,
                        GtkTreeIter *iter,
                        GtkTreePath *path)
{
  IdeTree *self = (IdeTree *)tree_view;
  IdeTreeModel *model;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE (self));
  g_assert (iter != NULL);
  g_assert (path != NULL);

  if (!(model = ide_tree_get_model (self)) ||
      !(node = ide_tree_model_get_node (IDE_TREE_MODEL (model), iter)))
    return;

  /*
   * If we are collapsing a row that requests to have its children removed
   * and the dummy node re-inserted, go ahead and do so now.
   */
  if (ide_tree_node_get_reset_on_collapse (node))
    _ide_tree_node_remove_all (node);

  _ide_tree_model_row_collapsed (model, self, path);
}

static void
ide_tree_popup (IdeTree        *self,
                IdeTreeNode    *node,
                GdkEventButton *event,
                gint            target_x,
                gint            target_y)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  const GdkRectangle area = { target_x, target_y, 0, 0 };
  GtkTextDirection dir;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  if (priv->context_menu == NULL)
    return;

  dir = gtk_widget_get_direction (GTK_WIDGET (self));

  if (priv->popover == NULL)
    {
      priv->popover = GTK_POPOVER (gtk_popover_new_from_model (GTK_WIDGET (self),
                                                               G_MENU_MODEL (priv->context_menu)));
      g_signal_connect (priv->popover,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &priv->popover);
    }

  gtk_popover_set_pointing_to (priv->popover, &area);
  gtk_popover_set_position (priv->popover, dir == GTK_TEXT_DIR_LTR ? GTK_POS_RIGHT : GTK_POS_LEFT);

  ide_tree_show_popover_at_node (self, node, priv->popover);
}

static gboolean
ide_tree_button_press_event (GtkWidget      *widget,
                             GdkEventButton *event)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreeModel *model;

  g_assert (IDE_IS_TREE (self));
  g_assert (event != NULL);

  if ((model = ide_tree_get_model (self)) &&
      (event->type == GDK_BUTTON_PRESS) &&
      (event->button == GDK_BUTTON_SECONDARY))
    {
      g_autoptr(GtkTreePath) path = NULL;
      gint cell_y;

      if (!gtk_widget_has_focus (GTK_WIDGET (self)))
        gtk_widget_grab_focus (GTK_WIDGET (self));

      gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (self),
                                     event->x,
                                     event->y,
                                     &path,
                                     NULL,
                                     NULL,
                                     &cell_y);

      if (path == NULL)
        {
          ide_tree_unselect (self);
        }
      else
        {
          GtkAllocation alloc;
          GtkTreeIter iter;

          gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
            {
              IdeTreeNode *node;

              node = ide_tree_model_get_node (IDE_TREE_MODEL (model), &iter);
              ide_tree_select (self, node);
              ide_tree_popup (self, node, event, alloc.x + alloc.width, event->y - cell_y);
            }
        }

      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (ide_tree_parent_class)->button_press_event (widget, event);
}

static gboolean
ide_tree_drag_motion (GtkWidget      *widget,
                      GdkDragContext *context,
                      gint            x,
                      gint            y,
                      guint           time_)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  gboolean ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (context != NULL);

  ret = GTK_WIDGET_CLASS (ide_tree_parent_class)->drag_motion (widget, context, x, y, time_);

  /*
   * Cache the current drop position so we can use it
   * later to determine how to drop on a given node.
   */
  g_clear_pointer (&priv->drop_path, gtk_tree_path_free);
  gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (self), &priv->drop_path, &priv->drop_pos);

  /* Save the drag action for builders dispatch */
  priv->drop_action = gdk_drag_context_get_selected_action (context);

  return ret;
}

static gboolean
ide_tree_query_tooltip (GtkWidget  *widget,
                        gint        x,
                        gint        y,
                        gboolean    keyboard_mode,
                        GtkTooltip *tooltip)
{
  GtkTreeView *tree_view = (GtkTreeView *)widget;
  g_autoptr(GtkTreePath) path = NULL;
  GtkTreeViewColumn *column;
  gint cell_x = 0;
  gint cell_y = 0;

  g_assert (IDE_IS_TREE (tree_view));
  g_assert (GTK_IS_TOOLTIP (tooltip));

  if (gtk_tree_view_get_path_at_pos (tree_view, x, y, &path, &column, &cell_x, &cell_y))
    {
      GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter (model, &iter, path))
        {
          IdeTreeNode *node;

          node = ide_tree_model_get_node (IDE_TREE_MODEL (model), &iter);

          if (node != NULL)
            {
              gtk_tooltip_set_text (tooltip,
                                    ide_tree_node_get_display_name (node));
              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
ide_tree_destroy (GtkWidget *widget)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  IdeTreeModel *model;

  g_assert (IDE_IS_MAIN_THREAD ());

  if ((model = ide_tree_get_model (self)))
    _ide_tree_model_release_addins (model);

  if (priv->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (priv->popover));

  gtk_tree_view_set_model (GTK_TREE_VIEW (self), NULL);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  g_clear_object (&priv->context_menu);

  g_clear_pointer (&priv->dim_label_attributes, pango_attr_list_unref);
  g_clear_pointer (&priv->header_attributes, pango_attr_list_unref);
  g_clear_pointer (&priv->drop_path, gtk_tree_path_free);

  GTK_WIDGET_CLASS (ide_tree_parent_class)->destroy (widget);
}

static void
ide_tree_class_init (IdeTreeClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkTreeViewClass *tree_view_class = GTK_TREE_VIEW_CLASS (klass);

  widget_class->destroy = ide_tree_destroy;
  widget_class->button_press_event = ide_tree_button_press_event;
  widget_class->drag_motion = ide_tree_drag_motion;
  widget_class->query_tooltip = ide_tree_query_tooltip;

  tree_view_class->row_activated = ide_tree_row_activated;
  tree_view_class->row_expanded = ide_tree_row_expanded;
  tree_view_class->row_collapsed = ide_tree_row_collapsed;
}

static void
ide_tree_init (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkCellRenderer *cell;
  GtkTreeViewColumn *column;

  priv->cancellable = g_cancellable_new ();

  gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);

  g_signal_connect_object (gtk_tree_view_get_selection (GTK_TREE_VIEW (self)),
                           "changed",
                           G_CALLBACK (ide_tree_selection_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self), FALSE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (self), TRUE);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  cell = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "xpad", 6,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, pixbuf_cell_func, self, NULL);

  cell = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, text_cell_func, self, NULL);

  cell = g_object_new (IDE_TYPE_CELL_RENDERER_STATUS, NULL);
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cell, state_cell_func, self, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);

  priv->dim_label_attributes = pango_attr_list_new ();
  pango_attr_list_insert (priv->dim_label_attributes,
                          pango_attr_foreground_alpha_new (65535 * 0.55));

  priv->header_attributes = pango_attr_list_new ();
  pango_attr_list_insert (priv->header_attributes,
                          pango_attr_weight_new (PANGO_WEIGHT_BOLD));
}

GtkWidget *
ide_tree_new (void)
{
  return g_object_new (IDE_TYPE_TREE, NULL);
}

void
ide_tree_set_context_menu (IdeTree *self,
                           GMenu   *menu)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (!menu || G_IS_MENU (menu));

  if (g_set_object (&priv->context_menu, menu))
    {
      if (priv->popover != NULL)
        gtk_widget_destroy (GTK_WIDGET (priv->popover));
    }

  g_return_if_fail (priv->popover == NULL);
}

void
ide_tree_show_popover_at_node (IdeTree     *self,
                               IdeTreeNode *node,
                               GtkPopover  *popover)
{
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  _ide_tree_node_show_popover (node, self, popover);
}

/**
 * ide_tree_get_selected_node:
 * @self: a #IdeTree
 *
 * Gets the currently selected node, or %NULL
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_get_selected_node (IdeTree *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));

  if (gtk_tree_selection_get_selected (selection, &model, &iter) && IDE_IS_TREE_MODEL (model))
    return ide_tree_model_get_node (IDE_TREE_MODEL (model), &iter);

  return NULL;
}

void
ide_tree_select_node (IdeTree     *self,
                      IdeTreeNode *node)
{
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (!node || IDE_IS_TREE_NODE (node));

  if (node == NULL)
    ide_tree_unselect (self);
  else
    ide_tree_select (self, node);
}

static void
ide_tree_expand_node_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeTreeModel *model = (IdeTreeModel *)object;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_tree_model_expand_finish (model, result, NULL))
    {
      g_autoptr(GtkTreePath) path = NULL;
      IdeTreeNode *node;
      IdeTree *self;

      self = ide_task_get_source_object (task);
      node = ide_task_get_task_data (task);

      g_assert (IDE_IS_TREE (self));
      g_assert (IDE_IS_TREE_NODE (node));

      if ((path = ide_tree_node_get_path (node)))
        gtk_tree_view_expand_row (GTK_TREE_VIEW (self), path, FALSE);
    }

  ide_task_return_boolean (task, TRUE);
}

void
ide_tree_expand_node (IdeTree     *self,
                      IdeTreeNode *node)
{
  g_autoptr(IdeTask) task = NULL;
  IdeTreeModel *model;

  g_return_if_fail (IDE_IS_TREE (self));

  if (!(model = ide_tree_get_model (self)))
    return;

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, ide_tree_expand_node);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  ide_tree_model_expand_async (model,
                               node,
                               NULL,
                               ide_tree_expand_node_cb,
                               g_steal_pointer (&task));
}

gboolean
ide_tree_node_expanded (IdeTree     *self,
                        IdeTreeNode *node)
{
  g_autoptr(GtkTreePath) path = NULL;

  g_return_val_if_fail (IDE_IS_TREE (self), FALSE);
  g_return_val_if_fail (!node || IDE_IS_TREE_NODE (node), FALSE);

  if (node == NULL)
    return FALSE;

  if (!(path = ide_tree_node_get_path (node)))
    return FALSE;

  return gtk_tree_view_row_expanded (GTK_TREE_VIEW (self), path);
}

void
ide_tree_collapse_node (IdeTree     *self,
                        IdeTreeNode *node)
{
  IdeTreeModel *model;
  g_autoptr(GtkTreePath) path = NULL;

  g_return_if_fail (IDE_IS_TREE (self));

  if (!(model = ide_tree_get_model (self)))
    return;

  if ((path = ide_tree_node_get_path (node)))
    gtk_tree_view_collapse_row (GTK_TREE_VIEW (self), path);
}

GdkDragAction
_ide_tree_get_drop_actions (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), 0);

  return priv->drop_action;
}

IdeTreeNode *
_ide_tree_get_drop_node (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(GtkTreePath) copy = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  if (priv->drop_path == NULL)
    return NULL;

  copy = gtk_tree_path_copy (priv->drop_path);

  if (priv->drop_pos == GTK_TREE_VIEW_DROP_BEFORE ||
      priv->drop_pos == GTK_TREE_VIEW_DROP_AFTER)
    {
      if (gtk_tree_path_get_depth (copy) > 1)
        gtk_tree_path_up (copy);
    }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self));

  if (gtk_tree_model_get_iter (model, &iter, copy))
    {
      IdeTreeNode *node = iter.user_data;

      if (IDE_IS_TREE_NODE (node))
        {
          if (ide_tree_node_is_empty (node))
            node = ide_tree_node_get_parent (node);
        }

      return node;
    }

  return NULL;
}
