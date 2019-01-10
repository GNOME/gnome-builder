/* ide-fancy-tree-view.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-fancy-tree-view"

#include "config.h"

#include <dazzle.h>

#include "ide-cell-renderer-fancy.h"
#include "ide-fancy-tree-view.h"

/**
 * SECTION:ide-fancy-tree-view:
 * @title: IdeFancyTreeView
 * @short_description: a stylized treeview for use in sidebars
 *
 * This is a helper #GtkTreeView that matches the style that
 * Builder uses for treeviews which can reflow text. It is a
 * useful base class because it does all of the hacks necessary
 * to make this work without ruining your code.
 *
 * It only has a single column, and comes setup with a single
 * cell (an #IdeCellRendererFancy) to render the conten.
 *
 * Since: 3.32
 */

typedef struct
{
  gint  last_width;
  guint relayout_source;
} IdeFancyTreeViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeFancyTreeView, ide_fancy_tree_view, GTK_TYPE_TREE_VIEW)

static void
ide_fancy_tree_view_destroy (GtkWidget *widget)
{
  IdeFancyTreeView *self = (IdeFancyTreeView *)widget;
  IdeFancyTreeViewPrivate *priv = ide_fancy_tree_view_get_instance_private (self);

  dzl_clear_source (&priv->relayout_source);

  GTK_WIDGET_CLASS (ide_fancy_tree_view_parent_class)->destroy (widget);
}

static gboolean
queue_relayout_in_idle (gpointer user_data)
{
  IdeFancyTreeView *self = user_data;
  IdeFancyTreeViewPrivate *priv = ide_fancy_tree_view_get_instance_private (self);
  GtkAllocation alloc;
  guint n_columns;

  g_assert (IDE_IS_FANCY_TREE_VIEW (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  if (alloc.width == priv->last_width)
    goto cleanup;

  priv->last_width = alloc.width;

  n_columns = gtk_tree_view_get_n_columns (GTK_TREE_VIEW (self));

  for (guint i = 0; i < n_columns; i++)
    {
      GtkTreeViewColumn *column;

      column = gtk_tree_view_get_column (GTK_TREE_VIEW (self), i);
      gtk_tree_view_column_queue_resize (column);
    }

cleanup:
  priv->relayout_source = 0;

  return G_SOURCE_REMOVE;
}


static void
ide_fancy_tree_view_size_allocate (GtkWidget *widget,
                                   GtkAllocation *alloc)
{
  IdeFancyTreeView *self = (IdeFancyTreeView *)widget;
  IdeFancyTreeViewPrivate *priv = ide_fancy_tree_view_get_instance_private (self);

  g_assert (IDE_IS_FANCY_TREE_VIEW (self));

  GTK_WIDGET_CLASS (ide_fancy_tree_view_parent_class)->size_allocate (widget, alloc);

  if (priv->last_width != alloc->width)
    {
      /*
       * We must perform our queued relayout from an idle callback
       * so that we don't affect this draw cycle. If we do that, we
       * will get empty content flashes for the current frame. This
       * allows us to draw the current frame slightly incorrect but
       * fixup on the next frame (which looks much nicer from a user
       * point of view).
       */
      if (priv->relayout_source == 0)
        priv->relayout_source =
          gdk_threads_add_idle_full (G_PRIORITY_HIGH,
                                     queue_relayout_in_idle,
                                     g_object_ref (self),
                                     g_object_unref);
    }
}

static void
ide_fancy_tree_view_class_init (IdeFancyTreeViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->size_allocate = ide_fancy_tree_view_size_allocate;
  widget_class->destroy = ide_fancy_tree_view_destroy;
}

static void
ide_fancy_tree_view_init (IdeFancyTreeView *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  g_object_set (self,
                "activate-on-single-click", TRUE,
                "headers-visible", FALSE,
                NULL);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "expand", TRUE,
                         "visible", TRUE,
                         NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (self), column);

  cell = g_object_new (IDE_TYPE_CELL_RENDERER_FANCY,
                       "visible", TRUE,
                       "xalign", 0.0f,
                       "xpad", 4,
                       "ypad", 6,
                       NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
}

GtkWidget *
ide_fancy_tree_view_new (void)
{
  return g_object_new (IDE_TYPE_FANCY_TREE_VIEW, NULL);
}

/**
 * ide_fancy_tree_view_set_data_func:
 * @self: a #IdeFancyTreeView
 * @func: (closure func_data) (scope async) (nullable): a callback
 * @func_data: data for @func
 * @func_data_destroy: destroy notify for @func_data
 *
 * Sets the data func to use to update the text for the
 * #IdeCellRendererFancy cell renderer.
 *
 * Since: 3.32
 */
void
ide_fancy_tree_view_set_data_func (IdeFancyTreeView      *self,
                                   GtkCellLayoutDataFunc  func,
                                   gpointer               func_data,
                                   GDestroyNotify         func_data_destroy)
{
  GtkTreeViewColumn *column;
  GList *cells;

  g_return_if_fail (IDE_IS_FANCY_TREE_VIEW (self));

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (self), 0);
  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));

  if (cells->data != NULL)
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), cells->data,
                                        func, func_data, func_data_destroy);

  g_list_free (cells);
}
