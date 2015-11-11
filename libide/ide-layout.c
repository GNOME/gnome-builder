/* ide-layout.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <egg-animation.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <string.h>

#include "ide-layout.h"
#include "ide-layout-pane.h"

#define ANIMATION_MODE     EGG_ANIMATION_EASE_IN_OUT_QUAD
#define ANIMATION_DURATION 250
#define HORIZ_GRIP_EXTRA   5
#define VERT_GRIP_EXTRA    5
#define MIN_POSITION       100

typedef struct
{
  GtkWidget       *widget;
  GtkAdjustment   *adjustment;
  EggAnimation    *animation;
  GdkWindow       *handle;
  GtkAllocation    handle_pos;
  GtkAllocation    alloc;
  gint             min_width;
  gint             min_height;
  gint             nat_width;
  gint             nat_height;
  gint             position;
  gint             restore_position;
  GdkCursorType    cursor_type;
  GtkPositionType  type : 4;
  guint            reveal : 1;
  guint            hiding : 1;
  guint            showing : 1;
} IdeLayoutChild;

typedef struct
{
  IdeLayoutChild    children[4];
  GtkGesture       *pan_gesture;
  IdeLayoutChild   *drag_child;
  gdouble           drag_position;
} IdeLayoutPrivate;

static void buildable_init_iface (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeLayout, ide_layout, GTK_TYPE_OVERLAY,
                         G_ADD_PRIVATE (IdeLayout)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_init_iface))

enum {
  PROP_0,
  PROP_BOTTOM_PANE,
  PROP_CONTENT_PANE,
  PROP_LEFT_PANE,
  PROP_RIGHT_PANE,
  LAST_PROP
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_REVEAL,
  CHILD_PROP_POSITION,
  LAST_CHILD_PROP
};

static GtkBuildableIface *ide_layout_parent_buildable_iface;
static GParamSpec        *properties [LAST_PROP];
static GParamSpec        *child_properties [LAST_CHILD_PROP];

static void
ide_layout_move_resize_handle (IdeLayout       *self,
                               GtkPositionType  type)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  IdeLayoutChild *child;
  GtkAllocation alloc;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert ((type == GTK_POS_LEFT) ||
            (type == GTK_POS_RIGHT) ||
            (type == GTK_POS_BOTTOM));

  child = &priv->children [type];

  if (child->handle == NULL)
    return;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  switch (type)
    {
    case GTK_POS_LEFT:
      child->handle_pos.x = alloc.x + child->alloc.x + child->alloc.width - HORIZ_GRIP_EXTRA;
      child->handle_pos.y = alloc.y + child->alloc.y;
      child->handle_pos.width = 2 * HORIZ_GRIP_EXTRA;
      child->handle_pos.height = child->alloc.height;
      break;

    case GTK_POS_RIGHT:
      child->handle_pos.x = alloc.x + child->alloc.x - HORIZ_GRIP_EXTRA;
      child->handle_pos.y = alloc.y + child->alloc.y;
      child->handle_pos.width = 2 * HORIZ_GRIP_EXTRA;
      child->handle_pos.height = child->alloc.height;
      break;

    case GTK_POS_BOTTOM:
      child->handle_pos.x = alloc.x + child->alloc.x;
      child->handle_pos.y = alloc.y + child->alloc.y - VERT_GRIP_EXTRA;
      child->handle_pos.width = child->alloc.width;
      child->handle_pos.height = 2 * VERT_GRIP_EXTRA;
      break;

    case GTK_POS_TOP:
    default:
      break;
    }

  if (!gtk_widget_get_child_visible (child->widget))
    memset (&child->handle_pos, 0, sizeof child->handle_pos);

  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    gdk_window_move_resize (child->handle,
                            child->handle_pos.x,
                            child->handle_pos.y,
                            child->handle_pos.width,
                            child->handle_pos.height);
}

static void
ide_layout_create_handle_window (IdeLayout       *self,
                                 GtkPositionType  type)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  IdeLayoutChild *child;
  GtkAllocation alloc;
  GdkWindowAttr attributes = { 0 };
  GdkWindow *parent;
  GdkDisplay *display;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert ((type == GTK_POS_LEFT) ||
            (type == GTK_POS_RIGHT) ||
            (type == GTK_POS_BOTTOM));

  display = gtk_widget_get_display (GTK_WIDGET (self));
  parent = gtk_widget_get_window (GTK_WIDGET (self));

  g_assert (GDK_IS_DISPLAY (display));
  g_assert (GDK_IS_WINDOW (parent));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  child = &priv->children [type];

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = child->handle_pos.x;
  attributes.y = child->handle_pos.y;
  attributes.width = child->handle_pos.width;
  attributes.height = child->handle_pos.height;
  attributes.visual = gtk_widget_get_visual (GTK_WIDGET (self));
  attributes.event_mask = gtk_widget_get_events (GTK_WIDGET (self));
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK |
                            GDK_POINTER_MOTION_MASK);
  attributes.cursor = gdk_cursor_new_for_display (display, child->cursor_type);

  child->handle = gdk_window_new (parent, &attributes, (GDK_WA_CURSOR | GDK_WA_X | GDK_WA_Y));
  gtk_widget_register_window (GTK_WIDGET (self), child->handle);

  g_clear_object (&attributes.cursor);
}

static void
ide_layout_destroy_handle_window (IdeLayout       *self,
                                  GtkPositionType  type)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  IdeLayoutChild *child;

  g_assert (IDE_IS_LAYOUT (self));

  child = &priv->children [type];

  if (child->handle)
    {
      gdk_window_hide (child->handle);
      gtk_widget_unregister_window (GTK_WIDGET (self), child->handle);
      gdk_window_destroy (child->handle);
      child->handle = NULL;
    }
}

static void
ide_layout_relayout (IdeLayout           *self,
                     const GtkAllocation *alloc)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  IdeLayoutChild *left;
  IdeLayoutChild *right;
  IdeLayoutChild *content;
  IdeLayoutChild *bottom;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (alloc != NULL);

  left = &priv->children [GTK_POS_LEFT];
  right = &priv->children [GTK_POS_RIGHT];
  content = &priv->children [GTK_POS_TOP];
  bottom = &priv->children [GTK_POS_BOTTOM];

  /*
   * Determine everything as if we are animating in/out or the child is visible.
   */

  if (left->reveal)
    {
      left->alloc.x = 0;
      left->alloc.y = 0;
      left->alloc.width = left->position;
      left->alloc.height = alloc->height;

      left->alloc.x -= gtk_adjustment_get_value (left->adjustment) * left->position;
    }
  else
    {
      left->alloc.x = -left->position;
      left->alloc.y = 0;
      left->alloc.width = left->position;
      left->alloc.height = alloc->height;
    }

  if (right->reveal)
    {
      right->alloc.x = alloc->width - right->position;
      right->alloc.y = 0;
      right->alloc.width = right->position;
      right->alloc.height = alloc->height;

      right->alloc.x += gtk_adjustment_get_value (right->adjustment) * right->position;
    }
  else
    {
      right->alloc.x = alloc->width;
      right->alloc.y = 0;
      right->alloc.width = right->position;
      right->alloc.height = alloc->height;
    }

  if (bottom->reveal)
    {
      bottom->alloc.x = left->alloc.x + left->alloc.width;
      bottom->alloc.y = alloc->height - bottom->position;
      bottom->alloc.width = right->alloc.x - bottom->alloc.x;
      bottom->alloc.height = bottom->position;

      bottom->alloc.y += gtk_adjustment_get_value (bottom->adjustment) * bottom->position;
    }
  else
    {
      bottom->alloc.x = left->alloc.x + left->alloc.width;
      bottom->alloc.y = alloc->height;
      bottom->alloc.width = right->alloc.x - bottom->alloc.x;
      bottom->alloc.height = bottom->position;
    }

  if (content->reveal)
    {
      content->alloc.x = left->alloc.x + left->alloc.width;
      content->alloc.y = 0;
      content->alloc.width = right->alloc.x - content->alloc.x;
      content->alloc.height = bottom->alloc.y;

      content->alloc.y -= gtk_adjustment_get_value (content->adjustment) * content->alloc.height;
    }
  else
    {
      content->alloc.x = left->alloc.x + left->alloc.width;
      content->alloc.y = -bottom->alloc.y;
      content->alloc.width = right->alloc.x - content->alloc.x;
      content->alloc.height = bottom->alloc.y;
    }

  /*
   * Now adjust for child visibility.
   *
   * We need to ensure we don't give the non-visible children an allocation
   * as it will interfere with hit targets.
   */
  if (!gtk_widget_get_child_visible (content->widget))
    memset (&content->alloc, 0, sizeof content->alloc);
  if (!gtk_widget_get_child_visible (left->widget))
    memset (&left->alloc, 0, sizeof left->alloc);
  if (!gtk_widget_get_child_visible (right->widget))
    memset (&right->alloc, 0, sizeof right->alloc);
  if (!gtk_widget_get_child_visible (bottom->widget))
    memset (&bottom->alloc, 0, sizeof bottom->alloc);

  ide_layout_move_resize_handle (self, GTK_POS_LEFT);
  ide_layout_move_resize_handle (self, GTK_POS_RIGHT);
  ide_layout_move_resize_handle (self, GTK_POS_BOTTOM);
}

static void
ide_layout_size_allocate (GtkWidget     *widget,
                          GtkAllocation *alloc)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gint i;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (alloc != NULL);

  ide_layout_relayout (self, alloc);

  GTK_WIDGET_CLASS (ide_layout_parent_class)->size_allocate (widget, alloc);

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      if ((child->handle != NULL) &&
          gtk_widget_get_visible (child->widget) &&
          gtk_widget_get_child_visible (child->widget))
        gdk_window_raise (child->handle);
    }
}

static IdeLayoutChild *
ide_layout_child_find (IdeLayout *self,
                       GtkWidget *child)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gint i;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *item = &priv->children [i];

      if (item->widget == child)
        return item;
    }

  g_warning ("Child of type %s was not found in this IdeLayout.",
             g_type_name (G_OBJECT_TYPE (child)));

  return NULL;
}

static void
ide_layout_animation_cb (gpointer data)
{
  g_autoptr(GtkWidget) child = data;
  GtkWidget *parent;
  IdeLayout *self;
  IdeLayoutChild *item;

  g_assert (GTK_IS_WIDGET (child));

  parent = gtk_widget_get_parent (child);
  if (!IDE_IS_LAYOUT (parent))
    return;

  self = IDE_LAYOUT (parent);

  item = ide_layout_child_find (self, child);
  if (item == NULL)
    return;

  if (item->hiding)
    {
      gtk_widget_set_child_visible (item->widget, FALSE);
      if (item->restore_position > item->position)
        item->position = item->restore_position;
    }

  item->showing = FALSE;
  item->hiding = FALSE;
  item->reveal = gtk_adjustment_get_value (item->adjustment) == 0.0;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  gtk_container_child_notify (GTK_CONTAINER (self), child, "reveal");
}

static gboolean
ide_layout_get_child_position (GtkOverlay    *overlay,
                               GtkWidget     *child,
                               GtkAllocation *alloc)
{
  IdeLayout *self = (IdeLayout *)overlay;
  IdeLayoutChild *item;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));
  g_assert (alloc != NULL);

  if (!(item = ide_layout_child_find (self, child)))
    return FALSE;

  *alloc = item->alloc;

  return TRUE;
}

static guint
ide_layout_child_get_position (IdeLayout *self,
                               GtkWidget *child)
{
  IdeLayoutChild *item;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = ide_layout_child_find (self, child)))
    return FALSE;

  return item->position;
}

static void
ide_layout_child_set_position (IdeLayout *self,
                               GtkWidget *child,
                               guint      position)
{
  IdeLayoutChild *item;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = ide_layout_child_find (self, child)))
    return;

  item->position = position;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  gtk_container_child_notify (GTK_CONTAINER (self), child, "position");
}

static gboolean
ide_layout_child_get_reveal (IdeLayout *self,
                             GtkWidget *child)
{
  IdeLayoutChild *item;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = ide_layout_child_find (self, child)))
    return FALSE;

  return item->reveal;
}

static void
ide_layout_child_set_reveal (IdeLayout *self,
                             GtkWidget *child,
                             gboolean   reveal)
{
  IdeLayoutChild *item;
  GdkFrameClock *frame_clock;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_WIDGET (child));

  reveal = !!reveal;

  if (!(item = ide_layout_child_find (self, child)) || (item->reveal == reveal))
    return;

  if (item->animation != NULL)
    {
      egg_animation_stop (item->animation);
      ide_clear_weak_pointer (&item->animation);
    }

  item->reveal = TRUE;
  item->showing = reveal;
  item->hiding = !reveal;

  if (item->position > MIN_POSITION)
    {
      item->restore_position = item->position;
      gtk_container_child_notify (GTK_CONTAINER (self), item->widget, "position");
    }

  gtk_widget_set_child_visible (child, TRUE);

  frame_clock = gtk_widget_get_frame_clock (child);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    {
      item->animation = egg_object_animate_full (item->adjustment,
                                                 ANIMATION_MODE,
                                                 ANIMATION_DURATION,
                                                 frame_clock,
                                                 ide_layout_animation_cb,
                                                 g_object_ref (child),
                                                 "value", reveal ? 0.0 : 1.0,
                                                 NULL);
      g_object_add_weak_pointer (G_OBJECT (item->animation), (gpointer *)&item->animation);
    }
  else
    {
      item->reveal = reveal;
      gtk_adjustment_set_value (item->adjustment, reveal ? 0.0 : 1.0);
      gtk_container_child_notify (GTK_CONTAINER (self), item->widget, "reveal");
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_layout_get_child_property (GtkContainer *container,
                               GtkWidget    *child,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  IdeLayout *self = (IdeLayout *)container;

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      g_value_set_boolean (value, ide_layout_child_get_reveal (self, child));
      break;

    case CHILD_PROP_POSITION:
      g_value_set_uint (value, ide_layout_child_get_position (self, child));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
ide_layout_set_child_property (GtkContainer *container,
                               GtkWidget    *child,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeLayout *self = (IdeLayout *)container;

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      ide_layout_child_set_reveal (self, child, g_value_get_boolean (value));
      break;

    case CHILD_PROP_POSITION:
      ide_layout_child_set_position (self, child, g_value_get_uint (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
ide_layout_get_preferred_width (GtkWidget *widget,
                                gint      *min_width,
                                gint      *nat_width)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gint i;

  g_assert (IDE_IS_LAYOUT (self));

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_width (child->widget, &child->min_width, &child->nat_width);
    }

  *min_width = priv->children [GTK_POS_LEFT].min_width
             + priv->children [GTK_POS_RIGHT].min_width
             + MAX (priv->children [GTK_POS_TOP].min_width,
                    priv->children [GTK_POS_BOTTOM].min_width);
  *nat_width = priv->children [GTK_POS_LEFT].nat_width
             + priv->children [GTK_POS_RIGHT].nat_width
             + MAX (priv->children [GTK_POS_TOP].nat_width,
                    priv->children [GTK_POS_BOTTOM].nat_width);
}

static void
ide_layout_get_preferred_height (GtkWidget *widget,
                                 gint      *min_height,
                                 gint      *nat_height)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gint i;

  g_assert (IDE_IS_LAYOUT (self));

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_height (child->widget, &child->min_height, &child->nat_height);
    }

  *min_height = MAX (MAX (priv->children [GTK_POS_LEFT].min_height,
                          priv->children [GTK_POS_RIGHT].min_height),
                     (priv->children [GTK_POS_BOTTOM].position +
                      priv->children [GTK_POS_TOP].min_height));

  *nat_height = MAX (MAX (priv->children [GTK_POS_LEFT].nat_height,
                          priv->children [GTK_POS_RIGHT].nat_height),
                     (priv->children [GTK_POS_BOTTOM].position +
                      priv->children [GTK_POS_TOP].nat_height));
}

static GtkSizeRequestMode
ide_layout_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static GtkAdjustment *
ide_layout_create_adjustment (IdeLayout *self)
{
  GtkAdjustment *adj;

  g_assert (IDE_IS_LAYOUT (self));

  adj = g_object_new (GTK_TYPE_ADJUSTMENT,
                      "lower", 0.0,
                      "upper", 1.0,
                      "value", 0.0,
                      NULL);

  g_signal_connect_object (adj,
                           "value-changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self,
                           G_CONNECT_SWAPPED);

  return adj;
}

static void
ide_layout_drag_begin_cb (IdeLayout     *self,
                          gdouble        x,
                          gdouble        y,
                          GtkGesturePan *pan)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  IdeLayoutChild *left;
  IdeLayoutChild *right;
  IdeLayoutChild *bottom;
  GdkEventSequence *sequence;
  const GdkEvent *event;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));

  left = &priv->children [GTK_POS_LEFT];
  right = &priv->children [GTK_POS_RIGHT];
  bottom = &priv->children [GTK_POS_BOTTOM];

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (pan));
  event = gtk_gesture_get_last_event (GTK_GESTURE (pan), sequence);

  if (event->any.window == left->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_HORIZONTAL);
      priv->drag_child = left;
    }
  else if (event->any.window == right->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_HORIZONTAL);
      priv->drag_child = right;
    }
  else if (event->any.window == bottom->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_VERTICAL);
      priv->drag_child = bottom;
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (pan), GTK_EVENT_SEQUENCE_DENIED);
      priv->drag_child = NULL;
      return;
    }

  priv->drag_position = MAX (priv->drag_child->position, MIN_POSITION);
  gtk_gesture_set_state (GTK_GESTURE (pan), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_container_child_notify (GTK_CONTAINER (self), priv->drag_child->widget, "position");
}

static void
ide_layout_drag_end_cb (IdeLayout     *self,
                        gdouble        x,
                        gdouble        y,
                        GtkGesturePan *pan)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  GdkEventSequence *sequence;
  GtkEventSequenceState state;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));

  if (priv->drag_child == NULL)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (pan));
  state = gtk_gesture_get_sequence_state (GTK_GESTURE (pan), sequence);
  if (state == GTK_EVENT_SEQUENCE_DENIED)
    {
      priv->drag_child = NULL;
      return;
    }

  if (priv->drag_child->position < MIN_POSITION)
    {
      gtk_container_child_set (GTK_CONTAINER (self), priv->drag_child->widget,
                               "reveal", FALSE,
                               NULL);
      priv->drag_child->restore_position = priv->drag_position;
    }

  gtk_container_child_notify (GTK_CONTAINER (self), priv->drag_child->widget, "position");

  priv->drag_child = NULL;
  priv->drag_position = 0;
}

static void
ide_layout_pan_cb (IdeLayout       *self,
                   GtkPanDirection  direction,
                   gdouble          offset,
                   GtkGesturePan   *pan)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  GtkAllocation alloc;
  gint target_position = 0;
  gint center_min_width;
  gint left_max;
  gint right_max;
  gint bottom_max;

  g_assert (IDE_IS_LAYOUT (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));
  g_assert (priv->drag_child != NULL);

  /*
   * NOTE: This is trickier than it looks, so I choose to be
   *       very verbose. Feel free to clean it up.
   */

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  switch (direction)
    {
    case GTK_PAN_DIRECTION_LEFT:
      if (priv->drag_child->type == GTK_POS_LEFT)
        target_position = priv->drag_position - offset;
      else if (priv->drag_child->type == GTK_POS_RIGHT)
        target_position = priv->drag_position + offset;
      break;

    case GTK_PAN_DIRECTION_RIGHT:
      if (priv->drag_child->type == GTK_POS_LEFT)
        target_position = priv->drag_position + offset;
      else if (priv->drag_child->type == GTK_POS_RIGHT)
        target_position = priv->drag_position - offset;
      break;

    case GTK_PAN_DIRECTION_UP:
      if (priv->drag_child->type == GTK_POS_BOTTOM)
        target_position = priv->drag_position + offset;
      break;

    case GTK_PAN_DIRECTION_DOWN:
      if (priv->drag_child->type == GTK_POS_BOTTOM)
        target_position = priv->drag_position - offset;
      break;

    default:
      g_assert_not_reached ();
    }

  center_min_width = MAX (priv->children [GTK_POS_BOTTOM].min_width,
                          priv->children [GTK_POS_TOP].min_width);
  left_max = alloc.width - priv->children [GTK_POS_RIGHT].alloc.width - center_min_width;
  right_max = alloc.width - priv->children [GTK_POS_LEFT].position - center_min_width;
  bottom_max = alloc.height - priv->children [GTK_POS_TOP].min_height;

  switch (priv->drag_child->type)
    {
    case GTK_POS_LEFT:
      target_position = MIN (left_max, target_position);
      break;

    case GTK_POS_RIGHT:
      target_position = MIN (right_max, target_position);
      break;

    case GTK_POS_BOTTOM:
      target_position = MIN (bottom_max, target_position);
      break;

    case GTK_POS_TOP:
    default:
      g_assert_not_reached ();
    }

  priv->drag_child->position = MAX (0, target_position);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static GtkGesture *
ide_layout_create_pan_gesture (IdeLayout      *self,
                               GtkOrientation  orientation)
{
  GtkGesture *gesture;

  g_assert (IDE_IS_LAYOUT (self));

  gesture = gtk_gesture_pan_new (GTK_WIDGET (self), orientation);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);

  g_signal_connect_object (gesture,
                           "drag-begin",
                           G_CALLBACK (ide_layout_drag_begin_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (gesture,
                           "drag-end",
                           G_CALLBACK (ide_layout_drag_end_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (gesture,
                           "pan",
                           G_CALLBACK (ide_layout_pan_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return gesture;
}

static void
ide_layout_realize (GtkWidget *widget)
{
  IdeLayout *self = (IdeLayout *)widget;

  g_assert (IDE_IS_LAYOUT (self));

  GTK_WIDGET_CLASS (ide_layout_parent_class)->realize (widget);

  ide_layout_create_handle_window (self, GTK_POS_LEFT);
  ide_layout_create_handle_window (self, GTK_POS_RIGHT);
  ide_layout_create_handle_window (self, GTK_POS_BOTTOM);
}

static void
ide_layout_unrealize (GtkWidget *widget)
{
  IdeLayout *self = (IdeLayout *)widget;

  g_assert (IDE_IS_LAYOUT (self));

  ide_layout_destroy_handle_window (self, GTK_POS_LEFT);
  ide_layout_destroy_handle_window (self, GTK_POS_RIGHT);
  ide_layout_destroy_handle_window (self, GTK_POS_BOTTOM);

  GTK_WIDGET_CLASS (ide_layout_parent_class)->unrealize (widget);
}

static void
ide_layout_map (GtkWidget *widget)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gint i;

  g_assert (IDE_IS_LAYOUT (self));

  GTK_WIDGET_CLASS (ide_layout_parent_class)->map (widget);

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      if (child->handle != NULL)
        gdk_window_show (child->handle);
    }
}

static void
ide_layout_unmap (GtkWidget *widget)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  int i;

  g_assert (IDE_IS_LAYOUT (self));

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      if (child->handle != NULL)
        gdk_window_hide (child->handle);
    }

  GTK_WIDGET_CLASS (ide_layout_parent_class)->unmap (widget);
}

static GObject *
ide_layout_get_internal_child (GtkBuildable *buildable,
                               GtkBuilder   *builder,
                               const gchar  *childname)
{
  IdeLayout *self = (IdeLayout *)buildable;

  g_assert (IDE_IS_LAYOUT (self));

  /*
   * Override default get_internal_child to handle RTL vs LTR.
   */
  if (ide_str_equal0 (childname, "left_pane"))
    return G_OBJECT (ide_layout_get_left_pane (self));
  else if (ide_str_equal0 (childname, "right_pane"))
    return G_OBJECT (ide_layout_get_right_pane (self));

  return ide_layout_parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
ide_layout_grab_focus (GtkWidget *widget)
{
  IdeLayout *self = (IdeLayout *)widget;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT (self));

  gtk_widget_grab_focus (priv->children [GTK_POS_TOP].widget);
}

static void
ide_layout_finalize (GObject *object)
{
  IdeLayout *self = (IdeLayout *)object;
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (priv->children); i++)
    {
      IdeLayoutChild *child = &priv->children [i];

      ide_clear_weak_pointer (&child->animation);
      g_clear_object (&child->adjustment);
    }

  g_clear_object (&priv->pan_gesture);

  G_OBJECT_CLASS (ide_layout_parent_class)->finalize (object);
}

static void
ide_layout_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeLayout *self = IDE_LAYOUT (object);

  switch (prop_id)
    {
    case PROP_LEFT_PANE:
      g_value_set_object (value, ide_layout_get_left_pane (self));
      break;

    case PROP_RIGHT_PANE:
      g_value_set_object (value, ide_layout_get_right_pane (self));
      break;

    case PROP_BOTTOM_PANE:
      g_value_set_object (value, ide_layout_get_bottom_pane (self));
      break;

    case PROP_CONTENT_PANE:
      g_value_set_object (value, ide_layout_get_content_pane (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
buildable_init_iface (GtkBuildableIface *iface)
{
  ide_layout_parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->get_internal_child = ide_layout_get_internal_child;
}

static void
ide_layout_class_init (IdeLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkOverlayClass *overlay_class = GTK_OVERLAY_CLASS (klass);

  object_class->finalize = ide_layout_finalize;
  object_class->get_property = ide_layout_get_property;
  object_class->set_property = ide_layout_set_property;

  widget_class->get_preferred_height = ide_layout_get_preferred_height;
  widget_class->get_preferred_width = ide_layout_get_preferred_width;
  widget_class->get_request_mode = ide_layout_get_request_mode;
  widget_class->map = ide_layout_map;
  widget_class->unmap = ide_layout_unmap;
  widget_class->realize = ide_layout_realize;
  widget_class->unrealize = ide_layout_unrealize;
  widget_class->size_allocate = ide_layout_size_allocate;
  widget_class->grab_focus = ide_layout_grab_focus;

  gtk_widget_class_set_css_name (widget_class, "layout");

  container_class->get_child_property = ide_layout_get_child_property;
  container_class->set_child_property = ide_layout_set_child_property;

  overlay_class->get_child_position = ide_layout_get_child_position;

  properties [PROP_LEFT_PANE] =
    g_param_spec_object ("left-pane",
                         "Left Pane",
                         "The left workspace pane.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RIGHT_PANE] =
    g_param_spec_object ("right-pane",
                         "Right Pane",
                         "The right workspace pane.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BOTTOM_PANE] =
    g_param_spec_object ("bottom-pane",
                         "Bottom Pane",
                         "The bottom workspace pane.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTENT_PANE] =
    g_param_spec_object ("content-pane",
                         "Content Pane",
                         "The content workspace pane.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  child_properties [CHILD_PROP_POSITION] =
    g_param_spec_uint ("position",
                       "Position",
                       "The position of the pane relative to its edge.",
                       0, G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gtk_container_class_install_child_property (container_class, CHILD_PROP_POSITION,
                                              child_properties [CHILD_PROP_POSITION]);

  child_properties [CHILD_PROP_REVEAL] =
    g_param_spec_boolean ("reveal",
                          "Reveal",
                          "If the pane should be revealed.",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gtk_container_class_install_child_property (container_class, CHILD_PROP_REVEAL,
                                              child_properties [CHILD_PROP_REVEAL]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout.ui");

  gtk_widget_class_bind_template_child_full (widget_class, "bottom_pane", TRUE,
                                             G_PRIVATE_OFFSET (IdeLayout, children[GTK_POS_BOTTOM].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "content_pane", TRUE,
                                             G_PRIVATE_OFFSET (IdeLayout, children[GTK_POS_TOP].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "left_pane", TRUE,
                                             G_PRIVATE_OFFSET (IdeLayout, children[GTK_POS_LEFT].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "right_pane", TRUE,
                                             G_PRIVATE_OFFSET (IdeLayout, children[GTK_POS_RIGHT].widget));
}

static void
ide_layout_init (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  priv->children [GTK_POS_LEFT].type = GTK_POS_LEFT;
  priv->children [GTK_POS_LEFT].reveal = TRUE;
  priv->children [GTK_POS_LEFT].position = 250;
  priv->children [GTK_POS_LEFT].adjustment = ide_layout_create_adjustment (self);
  priv->children [GTK_POS_LEFT].cursor_type = GDK_SB_H_DOUBLE_ARROW;

  priv->children [GTK_POS_RIGHT].type = GTK_POS_RIGHT;
  priv->children [GTK_POS_RIGHT].reveal = TRUE;
  priv->children [GTK_POS_RIGHT].position = 250;
  priv->children [GTK_POS_RIGHT].adjustment = ide_layout_create_adjustment (self);
  priv->children [GTK_POS_RIGHT].cursor_type = GDK_SB_H_DOUBLE_ARROW;

  priv->children [GTK_POS_BOTTOM].type = GTK_POS_BOTTOM;
  priv->children [GTK_POS_BOTTOM].reveal = TRUE;
  priv->children [GTK_POS_BOTTOM].position = 150;
  priv->children [GTK_POS_BOTTOM].adjustment = ide_layout_create_adjustment (self);
  priv->children [GTK_POS_BOTTOM].cursor_type = GDK_SB_V_DOUBLE_ARROW;

  priv->children [GTK_POS_TOP].type = GTK_POS_TOP;
  priv->children [GTK_POS_TOP].reveal = TRUE;
  priv->children [GTK_POS_TOP].adjustment = ide_layout_create_adjustment (self);

  priv->pan_gesture = ide_layout_create_pan_gesture (self, GTK_ORIENTATION_HORIZONTAL);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_layout_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT, NULL);
}

/**
 * ide_layout_get_left_pane:
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_layout_get_left_pane (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT (self), NULL);

  if (gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_DIR_RTL)
    return priv->children [GTK_POS_RIGHT].widget;
  else
    return priv->children [GTK_POS_LEFT].widget;
}

/**
 * ide_layout_get_right_pane:
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_layout_get_right_pane (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT (self), NULL);

  if (gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_DIR_RTL)
    return priv->children [GTK_POS_LEFT].widget;
  else
    return priv->children [GTK_POS_RIGHT].widget;
}

/**
 * ide_layout_get_bottom_pane:
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_layout_get_bottom_pane (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT (self), NULL);

  return priv->children [GTK_POS_BOTTOM].widget;
}

/**
 * ide_layout_get_content_pane:
 *
 * Returns: (transfer none): A #GtkWidget.
 */
GtkWidget *
ide_layout_get_content_pane (IdeLayout *self)
{
  IdeLayoutPrivate *priv = ide_layout_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT (self), NULL);

  return priv->children [GTK_POS_TOP].widget;
}
