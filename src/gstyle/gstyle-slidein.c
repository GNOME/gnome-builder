/* gstyle-slidein.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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
 * Initial ideas based on Gnome Builder Pnl dock system :
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gstyle-slidein"

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>

#include "gstyle-animation.h"
#include "gstyle-css-provider.h"
#include "gstyle-utils.h"

#include "gstyle-slidein.h"

struct _GstyleSlidein
{
  GtkEventBox                  parent_instance;

  GstyleCssProvider           *default_provider;
  GtkWidget                   *overlay_child;
  GdkWindow                   *overlay_window;

  gint64                       animation_starttime;
  gdouble                      offset;
  gdouble                      src_offset;
  gdouble                      dst_offset;
  gdouble                      slide_fraction;
  gdouble                      duration;
  guint                        slide_margin;

  gulong                       revealed_handler_id;
  gulong                       animation_handler_id;

  GstyleSlideinDirectionType   direction_type : 3;
  GstyleSlideinDirectionType   direction_type_reverse : 3;
  GstyleSlideinDirectionType   real_direction;
  guint                        interpolate_size : 1;
  guint                        revealed : 1;
  guint                        transition_done : 1;
  guint                        duration_set : 1;
  guint                        is_opening : 1;
  guint                        is_closing : 1;
};

static void gstyle_slidein_init_buildable_iface (GtkBuildableIface *iface);

G_DEFINE_TYPE_EXTENDED (GstyleSlidein, gstyle_slidein, GTK_TYPE_EVENT_BOX, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gstyle_slidein_init_buildable_iface))

enum {
  PROP_0,
  PROP_DIRECTION_TYPE,
  PROP_DURATION,
  PROP_INTERPOLATE_SIZE,
  PROP_SLIDE_FRACTION,
  PROP_SLIDE_MARGIN,
  PROP_REVEALED,
  N_PROPS
};

enum {
  CLOSING,
  OPENING,
  REVEALED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];

static guint       signals[N_SIGNALS];

static inline GtkOrientation
get_orientation (GstyleSlidein *self)
{
  if (self->direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_UP ||
      self->direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN)
    return GTK_ORIENTATION_VERTICAL;
  else
    return GTK_ORIENTATION_HORIZONTAL;
}

GstyleSlideinDirectionType
gstyle_slidein_get_direction_type (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE);

  return self->direction_type;
}

/**
 * gstyle_slidein_set_direction_type:
 * @direction_type: a #GstyleSlideinDirectionType
 *
 * Set the type of animation direction.
 *
 */
void
gstyle_slidein_set_direction_type (GstyleSlidein              *self,
                                   GstyleSlideinDirectionType  direction_type)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  if (self->direction_type != direction_type)
    {
      self->direction_type = direction_type;

      if (direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT)
          self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT;
      else if (direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT)
          self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT;
      else if (direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_UP)
          self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN;
      else if (direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN)
          self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_UP;
      else if (direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE)
          self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE;

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTION_TYPE]);
    }
}

/**
 * gstyle_slidein_set_interpolate_size:
 * @interpolate_size: %TRUE to interpolate the size
 *
 * Set whether or not we size the slidein according to all its children
 * (regular and slide) or just the regular one.
 *
 */
void
gstyle_slidein_set_interpolate_size (GstyleSlidein *self,
                                     gboolean       interpolate_size)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  if (self->interpolate_size != interpolate_size)
    {
      self->interpolate_size = interpolate_size;

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INTERPOLATE_SIZE]);
    }
}

/**
 * gstyle_slidein_get_interpolate_size:
 *
 * Get the interpolate-size set by gstyle_slidein_set_interpolate_size().
 *
 * Returns: %TRUE if we interpolate the size, %FALSE otherwise.
 */
gboolean
gstyle_slidein_get_interpolate_size (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), FALSE);

  return self->interpolate_size;
}

/**
 * gstyle_slidein_set_slide_fraction:
 * @slide_fraction: fraction of the slide compared to the total size
 *
 * Set the fraction used by the slide compared to the total size of the slidein,
 * in the direction of the animation (so horizontal or vertical).
 *
 */
void
gstyle_slidein_set_slide_fraction (GstyleSlidein *self,
                                   gdouble        slide_fraction)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  if (slide_fraction != self->slide_fraction)
    {
      self->slide_fraction = slide_fraction;

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SLIDE_FRACTION]);
    }
}

/**
 * gstyle_slidein_get_slide_fraction:
 *
 * Get the slide_fraction set by gstyle_slidein_set_slide_fraction().
 *
 * Returns: the slide_fraction.
 */
gdouble
gstyle_slidein_get_slide_fraction (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), 0);

  return self->slide_fraction;
}

/**
 * gstyle_slidein_set_slide_margin:
 * @slide_margin: margin.
 *
 * Set the margin left when the slide is opened, in pixels.
 */
void
gstyle_slidein_set_slide_margin (GstyleSlidein *self,
                                 guint          slide_margin)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  if (slide_margin != self->slide_margin)
    {
      self->slide_margin = slide_margin;

      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SLIDE_MARGIN]);
    }
}

/**
 * gstyle_slidein_get_slide_margin:
 *
 * Get the slide_margin set by gstyle_slidein_set_slide_margin().
 *
 * Returns: the slide_margin.
 */
guint
gstyle_slidein_get_slide_margin (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), 0);

  return self->slide_margin;
}

/**
 * gstyle_slidein_reset_duration:
 *
 * Reset the time taken to animate the slide to its default value.
 *
 */
void
gstyle_slidein_reset_duration (GstyleSlidein *self)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  self->duration = 0;
  self->duration_set = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
}

/**
 * gstyle_slidein_set_duration:
 * @duration: a time in ms
 *
 * Set the time taken to animation the slide  to the custom @duration value, in ms.
 *
 */
void
gstyle_slidein_set_duration (GstyleSlidein *self,
                             gdouble        duration)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  self->duration = duration;
  self->duration_set = TRUE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
}

/**
 * gstyle_slidein_get_duration:
 *
 * Get the time set to animate the slide.
 *
 * Returns: a time in ms.
 */
gdouble
gstyle_slidein_get_duration (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), 0.0);

  return self->duration;
}

static gdouble
compute_duration (GstyleSlidein *self)
{
  GtkWidget *child;
  GtkRequisition min_req_size;
  GtkRequisition nat_req_size;
  gdouble duration = 0.0;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  child = gtk_bin_get_child (GTK_BIN (self));
  gtk_widget_get_preferred_size (child, &min_req_size, &nat_req_size);

  if (get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
    duration = MAX (300, (nat_req_size.width - self->slide_margin) * self->slide_fraction * 1.2);
  else
    duration = MAX (300, (nat_req_size.height - self->slide_margin) * self->slide_fraction * 1.2);

  return duration;
}

static void
animate_stop (GstyleSlidein *self)
{
  g_assert (GSTYLE_IS_SLIDEIN (self));

  if (self->animation_handler_id)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->animation_handler_id);
      self->is_closing = self->is_opening = FALSE;
      self->animation_handler_id = 0;
    }
}

static void
animation_done_cb (GstyleSlidein *self)
{
  GstyleSlideinDirectionType animation_direction = GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  if (self->is_opening)
    {
      animate_stop (self);
      animation_direction = self->direction_type;
      self->revealed = TRUE;

      gtk_grab_add (GTK_WIDGET (self));
      gtk_event_box_set_above_child (GTK_EVENT_BOX (self), TRUE);
      gtk_widget_set_can_focus (self->overlay_child, TRUE);
      gtk_widget_grab_focus (self->overlay_child);
    }
  else if (self->is_closing)
    {
      animate_stop (self);
      animation_direction = self->direction_type_reverse;
      self->revealed = FALSE;

      gtk_event_box_set_above_child (GTK_EVENT_BOX (self), FALSE);
    }
  else
    g_assert_not_reached ();

  self->is_closing = self->is_opening = FALSE;
  self->offset = self->dst_offset;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REVEALED]);
  g_signal_emit (self, signals [REVEALED], 0, animation_direction, self->revealed);
}

static gboolean
animation_tick_cb (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       user_data)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;
  gint64 time;
  gdouble time_offset;
  gdouble ease_offset;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (frame_clock != NULL);

  if (!self->is_closing && !self->is_opening)
    return G_SOURCE_REMOVE;

  time = gdk_frame_clock_get_frame_time (frame_clock);
  time_offset = MIN ((time - self->animation_starttime) / ( 1000.0 * self->duration), 1.0);
  ease_offset = gstyle_animation_ease_in_out_cubic (time_offset);
  self->offset =  ease_offset * (self->dst_offset - self->src_offset) + self->src_offset;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  if (time_offset == 1.0)
    {
      animation_done_cb (self);
      return G_SOURCE_REMOVE;
    }
  else
    return G_SOURCE_CONTINUE;
}

/* src_offset and dst_offset need to be in [0.0, 1.0] range */
static gboolean
animate (GstyleSlidein *self,
         gdouble        target_offset)
{
  GtkWidget *child;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (0.0 <= target_offset && target_offset <= 1.0);

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child == NULL || self->overlay_child == NULL)
    return FALSE;

  animate_stop (self);

  if (!self->duration_set)
    self->duration = gstyle_animation_check_enable_animation () ? compute_duration (self) : 0;

  self->src_offset = self->offset;
  self->dst_offset = target_offset;
  gtk_widget_set_child_visible (child, TRUE);

  if (self->src_offset == self->dst_offset)
    return FALSE;

  if (self->src_offset < self->dst_offset)
    {
      self->is_opening = TRUE;
      g_signal_emit (self, signals [OPENING], 0);
    }
  else
    {
      self->is_closing = TRUE;
      g_signal_emit (self, signals [CLOSING], 0);
    }

  if (self->duration == 0)
    {
      self->offset = target_offset;
      animation_done_cb (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
  else if (!self->animation_handler_id)
    {
      self->animation_starttime = g_get_monotonic_time();
      self->animation_handler_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                                 animation_tick_cb,
                                                                 self,
                                                                 NULL);
    }

  return TRUE;
}

/**
 * gstyle_slidein_get_animation_state:
 * @direction: (out): slide animation state
 *
 * Get the animation state of the slide :
 * %TRUE if the slide is currently animated, %FALSE otherwise.
 *direction contains %TRUE if opening, %FALSE if closing.
 *
 * Returns: %TRUE if animating, %FALSE otherwise.
 *
 */
gboolean
gstyle_slidein_get_animation_state (GstyleSlidein *self,
                                    gboolean      *direction)
{
  gboolean is_animate;

  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), FALSE);

  is_animate = (self->is_opening || self->is_closing);
  if (is_animate)
    *direction = self->is_opening;
  else
    *direction = self->revealed;

  return is_animate;
}

/**
 * gstyle_slidein_reveal_slide:
 * @reveal: %TRUE to reveal or %FALSE to close the slide
 *
 * Reveal or close the slide.
 *
 * Returns: #TRUE if the action can be executed, otherwise, %FALSE if the slide is already
 *   in its final position, that is revealed or closed.
 */
gboolean
gstyle_slidein_reveal_slide (GstyleSlidein *self,
                             gboolean       reveal)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state = gtk_style_context_get_state (context);

  if (get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
    self->real_direction = (!!(state & GTK_STATE_FLAG_DIR_LTR)) ? self->direction_type
                                                                : self->direction_type_reverse;
  else
    self->real_direction = self->direction_type;

  return animate (self, reveal ? 1.0 : 0.0);
}

/**
 * gstyle_slidein_get_revealed:
 *
 * Get the state of the slide, revealed or not.
 *
 * Returns: %TRUE if the slide is already revealed, %FALSE otherwise.
 */
gboolean
gstyle_slidein_get_revealed (GstyleSlidein *self)
{
  g_return_val_if_fail (GSTYLE_IS_SLIDEIN (self), FALSE);

  return self->revealed;
}

static gboolean
gstyle_slidein_event_box_key_pressed_cb (GstyleSlidein *self,
                                         GdkEventKey   *event,
                                         GtkWidget     *widget)
{
  GtkWidget *focus;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  focus = gtk_window_get_focus (GTK_WINDOW (gtk_widget_get_toplevel (widget)));
  if (focus == NULL)
    return GDK_EVENT_PROPAGATE;

  if (event->keyval == GDK_KEY_Escape && !GTK_IS_ENTRY (focus))
    {
      gstyle_slidein_reveal_slide (self, FALSE);
      return GDK_EVENT_STOP;
    }

  if (gtk_widget_is_ancestor (focus, widget))
    return gtk_widget_event (focus, (GdkEvent*) event);

  return GDK_EVENT_PROPAGATE;
}

static void
gstyle_slidein_compute_child_allocation (GstyleSlidein *self,
                                         GtkAllocation  parent_alloc,
                                         GtkAllocation *child_alloc)
{
  GtkRequisition min_child_req, nat_child_req;
  gint slide_max_visible_size;
  gint margin;
  gint offset_x = 0;
  gint offset_y = 0;

  child_alloc->width = parent_alloc.width;
  child_alloc->height = parent_alloc.height;

  gtk_widget_get_preferred_size (self->overlay_child, &min_child_req, &nat_child_req);

  /* TODO: handle padding / margin */

  if (get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
    {
      margin = MIN (self->slide_margin, parent_alloc.width);
      slide_max_visible_size = parent_alloc.width - margin;
      child_alloc->width = MAX (MAX (slide_max_visible_size * self->slide_fraction, 1), min_child_req.width);

      if (self->real_direction == GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT)
        offset_x = parent_alloc.width - (child_alloc->width * self->offset) + 0.5;
      else
        offset_x = (self->offset - 1) * child_alloc->width + 0.5;
    }
  else
    {
      margin = MIN (self->slide_margin, parent_alloc.height);
      slide_max_visible_size = parent_alloc.height - margin;
      child_alloc->height =  MAX (MAX (slide_max_visible_size * self->slide_fraction, 1), min_child_req.height);

       if (self->direction_type == GSTYLE_SLIDEIN_DIRECTION_TYPE_UP)
        offset_y = parent_alloc.height - (child_alloc->height * self->offset) + 0.5;
      else
        offset_y = (self->offset - 1) * child_alloc->height + 0.5;
    }

  child_alloc->x = parent_alloc.x + offset_x;
  child_alloc->y = parent_alloc.y + offset_y;
}

static GdkWindow *
gstyle_slidein_create_child_window (GstyleSlidein *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkAllocation parent_alloc;
  GtkAllocation child_alloc;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (GTK_WIDGET (self), &parent_alloc);
  gstyle_slidein_compute_child_allocation (self, parent_alloc, &child_alloc);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.width = child_alloc.width;
  attributes.height = child_alloc.height;
  attributes.x = child_alloc.x;
  attributes.y = child_alloc.y;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
  attributes.event_mask = gtk_widget_get_events (widget);

  window = gdk_window_new (gtk_widget_get_window (widget), &attributes, attributes_mask);
  gtk_widget_register_window (widget, window);
  gtk_widget_set_parent_window (self->overlay_child, window);

  return window;
}

static gboolean
event_window_button_press_event_cb (GstyleSlidein *self,
                                    GdkEvent      *event,
                                    GstyleSlidein *unused)
{
  GdkEventButton *button_event = (GdkEventButton *)event;
  GtkAllocation child_alloc;
  gboolean is_in_slide;
  GtkWidget *src_widget;
  gint dest_x, dest_y;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  src_widget = gtk_get_event_widget (event);
  gtk_widget_translate_coordinates (src_widget, GTK_WIDGET (self->overlay_child),
                                    button_event->x, button_event->y,
                                    &dest_x, &dest_y);

  gtk_widget_get_allocated_size (self->overlay_child, &child_alloc, NULL);
  is_in_slide = (0 <= dest_x && dest_x <= child_alloc.width && 0 <= dest_y && dest_y <= child_alloc.height);
  if (!is_in_slide)
    {
      gtk_grab_remove (GTK_WIDGET (self));
      gstyle_slidein_reveal_slide (self, FALSE);

      return GDK_EVENT_PROPAGATE;
    }
  else
    return GDK_EVENT_STOP;
}

static void
gstyle_slidein_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GstyleSlidein *self = (GstyleSlidein *)container;
  gboolean was_visible = FALSE;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  if (widget == self->overlay_child)
    {
      if (self->overlay_window != NULL)
        {
          was_visible = gtk_widget_get_visible (widget);
          gtk_widget_unregister_window (GTK_WIDGET (container), self->overlay_window);
          gdk_window_destroy (self->overlay_window);
        }

      gtk_widget_unparent (widget);
      self->overlay_child = NULL;
      self->overlay_window = NULL;

      if (was_visible)
        gtk_widget_queue_resize(GTK_WIDGET(self));
    }
  else
    GTK_CONTAINER_CLASS (gstyle_slidein_parent_class)->remove (container, widget);
}

/**
 * gstyle_slidein_remove_slide:
 *
 * Remove, if any, the slide #GtkWidget.
 *
 */
void
gstyle_slidein_remove_slide (GstyleSlidein *self)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));

  if (self->overlay_child != NULL)
    gstyle_slidein_remove (GTK_CONTAINER (self), self->overlay_child);
}

/**
 * gstyle_slidein_add_slide:
 * @slide: a #GtkWidget
 *
 * Set the #GtkWidget to use as a slide.
 *
 */
void
gstyle_slidein_add_slide (GstyleSlidein *self,
                          GtkWidget     *slide)
{
  g_return_if_fail (GSTYLE_IS_SLIDEIN (self));
  g_return_if_fail (GTK_IS_WIDGET (slide));

  gstyle_slidein_remove_slide (self);

  /* TODO: cchoose only one naming: overlay or slide */
  self->overlay_child = slide;
  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    self->overlay_window = gstyle_slidein_create_child_window (self);

  gtk_widget_set_parent (slide, GTK_WIDGET (self));
  if(gtk_widget_get_visible(slide))
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

static void
gstyle_slidein_add_child (GtkBuildable *buildable,
                          GtkBuilder   *builder,
                          GObject      *child,
                          const gchar  *type)
{
  GstyleSlidein *self = (GstyleSlidein *)buildable;

  g_assert (GSTYLE_SLIDEIN (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (!GTK_IS_WIDGET (child))
    {
      g_warning ("Attempt to add a child of type \"%s\" to a \"%s\"",
                 G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (self));
      return;
    }

  if (type != NULL && g_strcmp0 (type, "slide") == 0)
    gstyle_slidein_add_slide (GSTYLE_SLIDEIN (buildable), GTK_WIDGET (child));
  else if (type == NULL)
    GTK_CONTAINER_CLASS (gstyle_slidein_parent_class)->add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

static void
gstyle_slidein_overlay_child_allocate (GstyleSlidein *self,
                                       GtkAllocation *alloc)
{
  GtkAllocation child_alloc = { 0, };
  gboolean visible;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (alloc != NULL);

  if (self->overlay_child != NULL)
    {
      visible = gtk_widget_get_visible (self->overlay_child);
       if (self->overlay_window != NULL && gtk_widget_get_mapped (GTK_WIDGET (self)))
        {
          if (visible)
            gdk_window_show (self->overlay_window);
          else if (gdk_window_is_visible (self->overlay_window))
            gdk_window_hide (self->overlay_window);
        }

      if (!visible)
        return;

      gstyle_slidein_compute_child_allocation (self, *alloc, &child_alloc);

      if (self->overlay_window != NULL)
        gdk_window_move_resize (self->overlay_window,
                                child_alloc.x, child_alloc.y,
                                child_alloc.width, child_alloc.height);

      child_alloc.x = 0;
      child_alloc.y = 0;
      gtk_widget_size_allocate (self->overlay_child, &child_alloc);
    }
}

static void
gstyle_slidein_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (allocation != NULL);

  GTK_WIDGET_CLASS (gstyle_slidein_parent_class)->size_allocate (widget, allocation);

  gstyle_slidein_overlay_child_allocate (self, allocation);
}

static void
gstyle_slidein_get_preferred_width (GtkWidget *widget,
                                    gint      *min_width,
                                    gint      *nat_width)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;
  GtkWidget *child;
  gint min_width_slide_based;
  gint nat_width_slide_based;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  *min_width = *nat_width = 1;

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child != NULL)
    gtk_widget_get_preferred_width (child, min_width, nat_width);

  if (self->interpolate_size ||
      (self->overlay_child != NULL && gtk_widget_get_visible (self->overlay_child)))
    {
      if (gtk_widget_get_request_mode (self->overlay_child) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
        {
          gint min_height;
          gint nat_height;

          gtk_widget_get_preferred_width (self->overlay_child, &min_height, &nat_height);
          GTK_WIDGET_GET_CLASS(self->overlay_child)->get_preferred_width_for_height (self->overlay_child,
                                                                                     min_height,
                                                                                     &min_width_slide_based,
                                                                                     &nat_width_slide_based);
        }
      else
        gtk_widget_get_preferred_width (self->overlay_child, &min_width_slide_based, &nat_width_slide_based);

      if (get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
        {
          if (!self->interpolate_size)
            {
              min_width_slide_based *= self->offset;
              nat_width_slide_based *= self->offset;
            }

          if (self->slide_fraction > 0)
            {
              min_width_slide_based /= self->slide_fraction;
              nat_width_slide_based /= self->slide_fraction;
            }

          min_width_slide_based += self->slide_margin;
          nat_width_slide_based += self->slide_margin;
        }

      /* TODO: add dynamic grow/shrink mode */
      *min_width = MAX (*min_width, min_width_slide_based);
      *nat_width = MAX (*nat_width, nat_width_slide_based);
    }
  else
    {
      *min_width = MAX (*min_width, self->slide_margin);
      *nat_width = MAX (*nat_width, self->slide_margin);
    }
}

static void
gstyle_slidein_get_preferred_height (GtkWidget *widget,
                                     gint      *min_height,
                                     gint      *nat_height)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;
  GtkWidget *child;
  gint min_height_slide_based;
  gint nat_height_slide_based;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  *min_height = *nat_height = 1;

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child != NULL)
    gtk_widget_get_preferred_width (child, min_height, nat_height);

  if (self->interpolate_size ||
      (self->overlay_child != NULL && gtk_widget_get_visible (self->overlay_child)))
    {
      if (gtk_widget_get_request_mode (self->overlay_child) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
        {
          gint min_width;
          gint nat_width;

          gtk_widget_get_preferred_width (self->overlay_child, &min_width, &nat_width);
          GTK_WIDGET_GET_CLASS(self->overlay_child)->get_preferred_height_for_width (self->overlay_child,
                                                                                     min_width,
                                                                                     &min_height_slide_based,
                                                                                     &nat_height_slide_based);
        }
      else
        gtk_widget_get_preferred_height (self->overlay_child, &min_height_slide_based, &nat_height_slide_based);

      if (get_orientation (self) == GTK_ORIENTATION_VERTICAL)
        {
          if (!self->interpolate_size)
            {
              min_height_slide_based *= self->offset;
              nat_height_slide_based *= self->offset;
            }

          if (self->slide_fraction > 0)
            {
              min_height_slide_based /= self->slide_fraction;
              nat_height_slide_based /= self->slide_fraction;
            }

          min_height_slide_based += self->slide_margin;
          nat_height_slide_based += self->slide_margin;
        }

      /* TODO: add dynamic grow/shrink mode */
      *min_height = MAX (*min_height, min_height_slide_based);
      *nat_height = MAX (*nat_height, nat_height_slide_based);
    }
  else
    {
      *min_height = MAX (*min_height, self->slide_margin);
      *nat_height = MAX (*nat_height, self->slide_margin);
    }
}

static void
gstyle_slidein_realize (GtkWidget *widget)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  GTK_WIDGET_CLASS (gstyle_slidein_parent_class)->realize (widget);
  gtk_widget_set_realized (widget, TRUE);

  if (self->overlay_child != NULL && self->overlay_window == NULL)
    self->overlay_window = gstyle_slidein_create_child_window (self);
}

static void
gstyle_slidein_unrealize (GtkWidget *widget)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  if (self->overlay_child != NULL && self->overlay_window != NULL)
    {
      gtk_widget_set_parent_window (self->overlay_child, NULL);
      gtk_widget_unregister_window (widget, self->overlay_window);
      gdk_window_destroy (self->overlay_window);
      self->overlay_window = NULL;
    }

  GTK_WIDGET_CLASS (gstyle_slidein_parent_class)->unrealize (widget);
}

static void
gstyle_slidein_map (GtkWidget *widget)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  GTK_WIDGET_CLASS (gstyle_slidein_parent_class)->map (widget);

  if (self->overlay_child != NULL &&
      self->overlay_window != NULL &&
      gtk_widget_get_visible (self->overlay_child) &&
      gtk_widget_get_child_visible (self->overlay_child))
    {
       gdk_window_show (self->overlay_window);
       g_signal_connect_swapped (self, "button-press-event",
                                 G_CALLBACK(event_window_button_press_event_cb),
                                 self);
    }
}

static void
gstyle_slidein_unmap (GtkWidget *widget)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  if (self->overlay_child != NULL &&
      self->overlay_window != NULL &&
      gtk_widget_is_visible (self->overlay_child))
    {
      gdk_window_hide (self->overlay_window );
      g_signal_handlers_disconnect_by_func (self->overlay_child,
                                            event_window_button_press_event_cb,
                                            self);
    }

  GTK_WIDGET_CLASS (gstyle_slidein_parent_class)->unmap (widget);
}

static gboolean
gstyle_slidein_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GstyleSlidein *self = (GstyleSlidein *)widget;
  GtkStyleContext *context;
  GtkAllocation shade_box;
  GtkWidget *child;
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_SLIDEIN (self));
  g_assert (cr != NULL);

  /* To draw the shade effect in between the regular child and the slides,
   * we bypass gtk_event_box_draw (we use a windowless one so not a problem),
   * and provide your own container draw implementation.
   */

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child == NULL)
    return GDK_EVENT_STOP;

  gtk_container_propagate_draw (GTK_CONTAINER (self), child, cr);

  if (self->offset > 0.0)
    {
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, "shade");
      gtk_style_context_get_color (context, gtk_style_context_get_state (context), &rgba);
      gtk_style_context_restore (context);
      rgba.alpha = rgba.alpha * self->offset;

      /* We shade the whole surface in case of slide tranparency */
      gtk_widget_get_allocated_size (widget, &shade_box, NULL);
      cairo_rectangle (cr, shade_box.x, shade_box.y, shade_box.width, shade_box.height);
      gdk_cairo_set_source_rgba (cr, &rgba);
      cairo_fill (cr);
    }

  if (self->overlay_child != NULL)
   gtk_container_propagate_draw (GTK_CONTAINER (self), self->overlay_child, cr);

  return GDK_EVENT_STOP;
}

static void
gstyle_slidein_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GstyleSlidein *self = (GstyleSlidein *)container;
  GtkWidget *child;

  g_assert (GSTYLE_IS_SLIDEIN (self));

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child)
    (* callback) (child, callback_data);

  if (self->overlay_child)
    (* callback) (self->overlay_child, callback_data);
}

GtkWidget *
gstyle_slidein_new (void)
{
  return g_object_new (GSTYLE_TYPE_SLIDEIN, NULL);
}

static void
gstyle_slidein_finalize (GObject *object)
{
  GstyleSlidein *self = (GstyleSlidein *)object;

  g_clear_object (&self->default_provider);
  g_clear_object (&self->overlay_child);

  G_OBJECT_CLASS (gstyle_slidein_parent_class)->finalize (object);
}

static void
gstyle_slidein_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GstyleSlidein *self = GSTYLE_SLIDEIN (object);

  switch (prop_id)
    {
    case PROP_DIRECTION_TYPE:
      g_value_set_enum (value, gstyle_slidein_get_direction_type (self));
      break;

    case PROP_DURATION:
      g_value_set_double (value, gstyle_slidein_get_duration (self));
      break;

    case PROP_INTERPOLATE_SIZE:
      g_value_set_boolean (value, gstyle_slidein_get_interpolate_size (self));
      break;

    case PROP_REVEALED:
      g_value_set_boolean (value, gstyle_slidein_get_revealed (self));
      break;

    case PROP_SLIDE_FRACTION:
      g_value_set_double (value, gstyle_slidein_get_slide_fraction (self));
      break;

    case PROP_SLIDE_MARGIN:
      g_value_set_uint (value, gstyle_slidein_get_slide_margin (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_slidein_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GstyleSlidein *self = GSTYLE_SLIDEIN (object);

  switch (prop_id)
    {
    case PROP_DIRECTION_TYPE:
      gstyle_slidein_set_direction_type (self, g_value_get_enum (value));
      break;

    case PROP_DURATION:
      gstyle_slidein_set_duration (self, g_value_get_double (value));
      break;

    case PROP_INTERPOLATE_SIZE:
      gstyle_slidein_set_interpolate_size (self, g_value_get_boolean (value));
      break;

    case PROP_SLIDE_FRACTION:
      gstyle_slidein_set_slide_fraction (self, g_value_get_double (value));
      break;

    case PROP_SLIDE_MARGIN:
      gstyle_slidein_set_slide_margin (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_slidein_class_init (GstyleSlideinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gstyle_slidein_finalize;
  object_class->get_property = gstyle_slidein_get_property;
  object_class->set_property = gstyle_slidein_set_property;

  widget_class->size_allocate = gstyle_slidein_size_allocate;
  widget_class->get_preferred_width = gstyle_slidein_get_preferred_width;
  widget_class->get_preferred_height = gstyle_slidein_get_preferred_height;
  widget_class->realize = gstyle_slidein_realize;
  widget_class->unrealize = gstyle_slidein_unrealize;
  widget_class->map = gstyle_slidein_map;
  widget_class->unmap = gstyle_slidein_unmap;
  widget_class->draw = gstyle_slidein_draw;

  container_class->remove = gstyle_slidein_remove;
  container_class->forall = gstyle_slidein_forall;

  properties [PROP_DURATION] =
    g_param_spec_double ("duration",
                         "duration",
                         "slide animation time in ms",
                         0.0,
                         G_MAXDOUBLE,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INTERPOLATE_SIZE] =
    g_param_spec_boolean ("interpolate-size",
                          "interpolate-size",
                          "interpolate-size",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SLIDE_FRACTION] =
    g_param_spec_double ("slide-fraction",
                         "slide-fraction",
                         "fraction to show when releaving the slide",
                         0.0,
                         1.0,
                         1.0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SLIDE_MARGIN] =
    g_param_spec_uint ("slide-margin",
                       "slide-margin",
                       "margin to keep when releaving the slide",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_REVEALED] =
    g_param_spec_boolean ("revealed",
                          "revealed",
                          "Whether the slidein is revealed",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DIRECTION_TYPE] =
    g_param_spec_enum ("direction-type",
                       "direction-type",
                       "direction-type",
                       GSTYLE_TYPE_SLIDEIN_DIRECTION_TYPE,
                       GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * GstyleSlidein::revealed:
   * @self: #GstyleSlidein instance.
   * @direction_type: type of opening direction.
   * @revealed: the revealed state of the slide.
   *
   * Emitted when the revealed state of the slide child change.
   * Notice that this only append after the duration time
   */
  signals[REVEALED] = g_signal_new ("revealed",
                                    GSTYLE_TYPE_SLIDEIN,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE,
                                    2,
                                    GSTYLE_TYPE_SLIDEIN_DIRECTION_TYPE,
                                    G_TYPE_BOOLEAN);

  /**
   * GstyleSlidein::closing:
   * @self: #GstyleSlidein instance.
   *
   * Emitted when the slide start closing.
   */
  signals[CLOSING] = g_signal_new ("closing",
                                   GSTYLE_TYPE_SLIDEIN,
                                   G_SIGNAL_RUN_FIRST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   0);

  /**
   * GstyleSlidein::opening:
   * @self: #GstyleSlidein instance.
   *
   * Emitted when the slide start opening.
   */
  signals[OPENING] = g_signal_new ("opening",
                                   GSTYLE_TYPE_SLIDEIN,
                                   G_SIGNAL_RUN_FIRST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   0);

  gtk_widget_class_set_css_name (widget_class, "gstyleslidein");
}

static void
gstyle_slidein_init (GstyleSlidein *self)
{
  GtkStyleContext *context;

  g_signal_connect_swapped (self,
                            "key-press-event",
                            G_CALLBACK (gstyle_slidein_event_box_key_pressed_cb),
                            self);

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (self), FALSE);
  gtk_event_box_set_above_child (GTK_EVENT_BOX (self), FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));

  self->direction_type = GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT;
  self->direction_type_reverse = GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT;
  self->duration = 0.0;
  self->duration_set = TRUE;
}

static void
gstyle_slidein_init_buildable_iface (GtkBuildableIface *iface)
{
  iface->add_child = gstyle_slidein_add_child;
}

GType
gstyle_slidein_type_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE, "GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE", "none" },
    { GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT, "GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT", "right" },
    { GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT, "GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT", "left" },
    { GSTYLE_SLIDEIN_DIRECTION_TYPE_UP, "GSTYLE_SLIDEIN_DIRECTION_TYPE_UP", "up" },
    { GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN, "GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN", "down" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleSlideinDirectionType", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
