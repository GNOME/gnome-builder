/* gstyle-revealer.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

/*
 * This revealer is meant to stay an internal one and deal with specific color panel problems.
 * and it only contain code to handle DOWN direction :
 *   - specific handling of a GstyleColorWidget (containing a scrolled window)
 *   - duration coded in hard.
 *
 * although not perfect because we don't control the container and children position,
 * it's a very aceptable solution.
 */

#include "gstyle-animation.h"
#include "gstyle-palette-widget.h"

#include "gstyle-revealer.h"

struct _GstyleRevealer
{
  GtkBin     parent_instance;

  GdkWindow *window;
  gdouble    duration;
  gdouble    offset;
  gdouble    src_offset;
  gdouble    dst_offset;
  gulong     animation_handler_id;
  gint64     animation_starttime;
  gdouble    previous_height;
  gint       max_height;

  guint      revealed : 1;
  guint      duration_set : 1;
  guint      is_animating : 1;
};

/* TODO: use spped instead of duration */

G_DEFINE_TYPE (GstyleRevealer, gstyle_revealer, GTK_TYPE_BIN)

enum {
  PROP_0,
  N_PROPS
};

#define GSTYLE_REVEALER_DEFAULT_DURATION 500

static void
animate_stop (GstyleRevealer *self)
{
  g_assert (GSTYLE_IS_REVEALER (self));

  if (self->animation_handler_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->animation_handler_id);
      self->is_animating = FALSE;
      self->animation_handler_id = 0;
    }
}

static gboolean
animation_tick_cb (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       user_data)
{
  GstyleRevealer *self = (GstyleRevealer *)widget;
  GtkWidget *child;
  gint64 time;
  gdouble time_offset;
  gdouble ease_offset;

  g_assert (GSTYLE_IS_REVEALER (self));
  g_assert (frame_clock != NULL);

  if (!self->is_animating)
    return G_SOURCE_REMOVE;

  time = gdk_frame_clock_get_frame_time (frame_clock);
  time_offset = MIN ((time - self->animation_starttime) / ( 1000.0 * self->duration), 1.0);
  ease_offset = gstyle_animation_ease_in_out_cubic (time_offset);
  self->offset =  ease_offset * (self->dst_offset - self->src_offset) + self->src_offset;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  if (time_offset == 1.0)
    {
      animate_stop (self);
      self->offset = self->dst_offset;
      self->revealed = !!(self->offset);

      child = gtk_bin_get_child (GTK_BIN (self));
      if (child != NULL)
        gtk_widget_set_child_visible (child, self->revealed);

      return G_SOURCE_REMOVE;
    }
  else
    return G_SOURCE_CONTINUE;
}

void
gstyle_revealer_set_reveal_child (GstyleRevealer *self,
                                  gboolean        reveal)
{
  GtkWidget *child;
  GtkAllocation allocation;

  g_return_if_fail (GSTYLE_IS_REVEALER (self));

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child == NULL || (!self->is_animating && reveal == self->revealed))
    return;

  animate_stop (self);

  if (!self->duration_set)
    self->duration = gstyle_animation_check_enable_animation () ? GSTYLE_REVEALER_DEFAULT_DURATION : 0;

  if (reveal)
    {
      self->src_offset = self->offset;
      self->dst_offset = 1.0;
    }
  else
    {
      self->src_offset = self->offset;
      self->dst_offset = 0.0;
    }

  if (GSTYLE_IS_PALETTE_WIDGET (child))
    {
      gtk_widget_get_allocated_size (GTK_WIDGET (self), &allocation, NULL);
      self->max_height = allocation.height;
    }
  else
    self->max_height = G_MAXINT;

  gtk_widget_set_child_visible (child, TRUE);

  if (self->duration == 0)
    {
      animate_stop (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
  else if (!self->animation_handler_id)
    {
      self->animation_starttime = g_get_monotonic_time();
      self->animation_handler_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), animation_tick_cb, self, NULL);
      self->is_animating = TRUE;
    }
}

static void
gstyle_revealer_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GstyleRevealer *self = (GstyleRevealer *)widget;
  GtkRequisition min_child_req, nat_child_req;
  GtkAllocation child_allocation;
  GtkWidget *child;

  g_assert (GSTYLE_IS_REVEALER (self));
  g_assert (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);
  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    gdk_window_move_resize (self->window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child == NULL)
    return;

  if (!gtk_widget_get_visible (child))
    return;

  gtk_widget_get_preferred_size (child, &min_child_req, &nat_child_req);

  child_allocation.width = allocation->width;
  child_allocation.height = MAX (min_child_req.height, allocation->height);
  child_allocation.x = 0;

  if (GSTYLE_IS_PALETTE_WIDGET (child))
    {
      if (nat_child_req.height > allocation->height)
        child_allocation.y = allocation->height * (self->offset - 1.0);
      else
        child_allocation.y = nat_child_req.height * (self->offset - 1.0);
    }
  else
    child_allocation.y = 0;

  gtk_widget_size_allocate (child, &child_allocation);

  allocation->y = 0;
  gtk_widget_set_clip (child, allocation);
}

GstyleRevealer *
gstyle_revealer_new (void)
{
  return g_object_new (GSTYLE_TYPE_REVEALER, NULL);
}

static void
gstyle_revealer_get_preferred_width (GtkWidget *widget,
                                     gint      *min_width,
                                     gint      *nat_width)
{
  GstyleRevealer *self = (GstyleRevealer *)widget;
  GtkWidget *child;

  g_assert (GSTYLE_IS_REVEALER (self));

  *min_width = *nat_width = 1;

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child != NULL)
    gtk_widget_get_preferred_width (child, min_width, nat_width);
}

static void
gstyle_revealer_get_preferred_height (GtkWidget *widget,
                                      gint      *min_height,
                                      gint      *nat_height)
{
  GstyleRevealer *self = (GstyleRevealer *)widget;
  gint child_min_height;
  gint child_nat_height;

  g_assert (GSTYLE_IS_REVEALER (self));

  GTK_WIDGET_CLASS (gstyle_revealer_parent_class)->get_preferred_height (widget, &child_min_height, &child_nat_height);

  *min_height = 0;
  *nat_height = MIN (self->max_height, child_nat_height) * self->offset;
}

static void
gstyle_revealer_get_preferred_height_for_width (GtkWidget *widget,
                                                gint       width,
                                                gint      *min_height,
                                                gint      *nat_height)
{
  gstyle_revealer_get_preferred_height (widget, min_height, nat_height);
}

static void
gstyle_revealer_realize (GtkWidget *widget)
{
  GstyleRevealer *self = (GstyleRevealer *)widget;
  GtkAllocation alloc;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_assert (GSTYLE_IS_REVEALER (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.width = alloc.width;
  attributes.height = alloc.height;
  attributes.x = alloc.x;
  attributes.y = alloc.y;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
  attributes.event_mask = gtk_widget_get_events (widget);

  self->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gtk_widget_register_window (widget, self->window);
  gtk_widget_set_window (GTK_WIDGET (self), self->window);

  gtk_widget_set_realized (widget, TRUE);
}

static void
gstyle_revealer_add (GtkContainer *container,
                     GtkWidget    *child)
{
  GstyleRevealer *self = (GstyleRevealer *)container;

  g_assert (GSTYLE_IS_REVEALER (self));

  gtk_widget_set_parent_window (child, self->window);
  gtk_widget_set_child_visible (child, self->revealed);

  GTK_CONTAINER_CLASS (gstyle_revealer_parent_class)->add (container, child);
}

static void
gstyle_revealer_class_init (GstyleRevealerClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  container_class->add = gstyle_revealer_add;

  widget_class->size_allocate = gstyle_revealer_size_allocate;
  widget_class->get_preferred_width = gstyle_revealer_get_preferred_width;
  widget_class->get_preferred_height = gstyle_revealer_get_preferred_height;
  widget_class->get_preferred_height_for_width = gstyle_revealer_get_preferred_height_for_width;
  widget_class->realize = gstyle_revealer_realize;
}

static void
gstyle_revealer_init (GstyleRevealer *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);

  self->duration = 500.0;
  self->duration_set = TRUE;
  self->max_height = G_MAXINT;
}
