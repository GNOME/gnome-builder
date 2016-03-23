/* pnl-util.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "pnl-util-private.h"

static void
pnl_gtk_border_sum (GtkBorder       *one,
                    const GtkBorder *two)
{
  one->top += two->top;
  one->right += two->right;
  one->bottom += two->bottom;
  one->left += two->left;
}

gboolean
pnl_gtk_bin_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkStyleContext *style_context;
  GtkStateFlags state;
  GtkAllocation alloc;
  GtkBorder border;
  GtkBorder padding;
  GtkWidget *child;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (cr != NULL);

  gtk_widget_get_allocation (widget, &alloc);

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (style_context);
  gtk_style_context_get_border (style_context, state, &border);
  gtk_style_context_get_padding (style_context, state, &padding);

  pnl_gtk_border_sum (&border, &padding);

  gtk_render_background (gtk_widget_get_style_context (widget), cr,
                         border.left,
                         border.top,
                         alloc.width - border.left - border.right,
                         alloc.height - border.top - border.bottom);

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child != NULL)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  return GDK_EVENT_PROPAGATE;
}

void
pnl_gtk_bin_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkStyleContext *style_context;
  GtkStateFlags state;
  GtkBorder border;
  GtkBorder padding;
  gint border_width;
  GtkWidget *child;

  g_return_if_fail (GTK_IS_BIN (widget));
  g_return_if_fail (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child == NULL)
    return;

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_style_context_get_state (style_context);
  gtk_style_context_get_border (style_context, state, &border);
  gtk_style_context_get_padding (style_context, state, &padding);

  pnl_gtk_border_sum (&border, &padding);

  allocation->x += border.left;
  allocation->y += border.top;
  allocation->width -= border.left + border.right;
  allocation->height -= border.top + border.bottom;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  allocation->x += border_width;
  allocation->y += border_width;
  allocation->width -= border_width * 2;
  allocation->height -= border_width * 2;

  gtk_widget_size_allocate (child, allocation);
}
