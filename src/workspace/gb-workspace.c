/* gb-workspace.c
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

#include <glib/gi18n.h>
#include <ide.h>
#include <string.h>

#include "gb-workspace.h"
#include "gb-workspace-pane.h"

#define ANIMATION_MODE     IDE_ANIMATION_EASE_IN_OUT_QUAD
#define ANIMATION_DURATION 250
#define HORIZ_GRIP_EXTRA   10
#define VERT_GRIP_EXTRA    10
#define MIN_POSITION       100

typedef struct
{
  GtkWidget       *widget;
  GtkAdjustment   *adjustment;
  IdeAnimation    *animation;
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
} GbWorkspaceChild;

struct _GbWorkspace
{
  GtkOverlay        parent_instance;

  GbWorkspaceChild  children[4];

  GtkGesture       *pan_gesture;

  GbWorkspaceChild *drag_child;
  gdouble           drag_position;
};

static void buildable_init_iface (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GbWorkspace, gb_workspace, GTK_TYPE_OVERLAY,
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

static GtkBuildableIface *gb_workspace_parent_buildable_iface;
static GParamSpec *gParamSpecs [LAST_PROP];
static GParamSpec *gChildParamSpecs [LAST_CHILD_PROP];

static void
gb_workspace_move_resize_handle (GbWorkspace     *self,
                                 GtkPositionType  type)
{
  GbWorkspaceChild *child;
  GtkAllocation alloc;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert ((type == GTK_POS_LEFT) ||
            (type == GTK_POS_RIGHT) ||
            (type == GTK_POS_BOTTOM));

  child = &self->children [type];

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
gb_workspace_create_handle_window (GbWorkspace     *self,
                                   GtkPositionType  type)
{
  GbWorkspaceChild *child;
  GtkAllocation alloc;
  GdkWindowAttr attributes = { 0 };
  GdkWindow *parent;
  GdkDisplay *display;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert ((type == GTK_POS_LEFT) ||
            (type == GTK_POS_RIGHT) ||
            (type == GTK_POS_BOTTOM));

  display = gtk_widget_get_display (GTK_WIDGET (self));
  parent = gtk_widget_get_window (GTK_WIDGET (self));

  g_assert (GDK_IS_DISPLAY (display));
  g_assert (GDK_IS_WINDOW (parent));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  child = &self->children [type];

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
gb_workspace_destroy_handle_window (GbWorkspace     *self,
                                    GtkPositionType  type)
{
  GbWorkspaceChild *child;

  g_assert (GB_IS_WORKSPACE (self));

  child = &self->children [type];

  if (child->handle)
    {
      gdk_window_hide (child->handle);
      gtk_widget_unregister_window (GTK_WIDGET (self), child->handle);
      gdk_window_destroy (child->handle);
      child->handle = NULL;
    }
}

static void
gb_workspace_relayout (GbWorkspace         *self,
                       const GtkAllocation *alloc)
{
  GbWorkspaceChild *left;
  GbWorkspaceChild *right;
  GbWorkspaceChild *content;
  GbWorkspaceChild *bottom;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (alloc != NULL);

  left = &self->children [GTK_POS_LEFT];
  right = &self->children [GTK_POS_RIGHT];
  content = &self->children [GTK_POS_TOP];
  bottom = &self->children [GTK_POS_BOTTOM];

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

  gb_workspace_move_resize_handle (self, GTK_POS_LEFT);
  gb_workspace_move_resize_handle (self, GTK_POS_RIGHT);
  gb_workspace_move_resize_handle (self, GTK_POS_BOTTOM);
}

static void
gb_workspace_size_allocate (GtkWidget     *widget,
                            GtkAllocation *alloc)
{
  GbWorkspace *self = (GbWorkspace *)widget;
  int i;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (alloc != NULL);

  gb_workspace_relayout (self, alloc);

  GTK_WIDGET_CLASS (gb_workspace_parent_class)->size_allocate (widget, alloc);

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      if ((child->handle != NULL) &&
          gtk_widget_get_visible (child->widget) &&
          gtk_widget_get_child_visible (child->widget))
        gdk_window_raise (child->handle);
    }
}

static GbWorkspaceChild *
gb_workspace_child_find (GbWorkspace *self,
                         GtkWidget   *child)
{
  int i;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *item = &self->children [i];

      if (item->widget == child)
        return item;
    }

  g_warning ("Child of type %s was not found in this GbWorkspace.",
             g_type_name (G_OBJECT_TYPE (child)));

  return NULL;
}

static void
gb_workspace_animation_cb (gpointer data)
{
  g_autoptr(GtkWidget) child = data;
  GtkWidget *parent;
  GbWorkspace *self;
  GbWorkspaceChild *item;

  g_assert (GTK_IS_WIDGET (child));

  parent = gtk_widget_get_parent (child);
  if (!GB_IS_WORKSPACE (parent))
    return;

  self = GB_WORKSPACE (parent);

  item = gb_workspace_child_find (self, child);
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
gb_workspace_get_child_position (GtkOverlay    *overlay,
                                 GtkWidget     *child,
                                 GtkAllocation *alloc)
{
  GbWorkspace *self = (GbWorkspace *)overlay;
  GbWorkspaceChild *item;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));
  g_assert (alloc != NULL);

  if (!(item = gb_workspace_child_find (self, child)))
    return FALSE;

  *alloc = item->alloc;

  return TRUE;
}

static guint
gb_workspace_child_get_position (GbWorkspace *self,
                                 GtkWidget   *child)
{
  GbWorkspaceChild *item;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = gb_workspace_child_find (self, child)))
    return FALSE;

  return item->position;
}

static void
gb_workspace_child_set_position (GbWorkspace *self,
                                 GtkWidget   *child,
                                 guint        position)
{
  GbWorkspaceChild *item;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = gb_workspace_child_find (self, child)))
    return;

  item->position = position;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  gtk_container_child_notify (GTK_CONTAINER (self), child, "position");
}

static gboolean
gb_workspace_child_get_reveal (GbWorkspace *self,
                               GtkWidget   *child)
{
  GbWorkspaceChild *item;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));

  if (!(item = gb_workspace_child_find (self, child)))
    return FALSE;

  return item->reveal;
}

static void
gb_workspace_child_set_reveal (GbWorkspace *self,
                               GtkWidget   *child,
                               gboolean     reveal)
{
  GbWorkspaceChild *item;
  GdkFrameClock *frame_clock;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (child));

  reveal = !!reveal;

  if (!(item = gb_workspace_child_find (self, child)) || (item->reveal == reveal))
    return;

  if (item->animation != NULL)
    {
      ide_animation_stop (item->animation);
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

  item->animation = ide_object_animate_full (item->adjustment,
                                             ANIMATION_MODE,
                                             ANIMATION_DURATION,
                                             frame_clock,
                                             gb_workspace_animation_cb,
                                             g_object_ref (child),
                                             "value", reveal ? 0.0 : 1.0,
                                             NULL);
  g_object_add_weak_pointer (G_OBJECT (item->animation), (gpointer *)&item->animation);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gb_workspace_get_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GbWorkspace *self = (GbWorkspace *)container;

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      g_value_set_boolean (value, gb_workspace_child_get_reveal (self, child));
      break;

    case CHILD_PROP_POSITION:
      g_value_set_uint (value, gb_workspace_child_get_position (self, child));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
gb_workspace_set_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbWorkspace *self = (GbWorkspace *)container;

  switch (prop_id)
    {
    case CHILD_PROP_REVEAL:
      gb_workspace_child_set_reveal (self, child, g_value_get_boolean (value));
      break;

    case CHILD_PROP_POSITION:
      gb_workspace_child_set_position (self, child, g_value_get_uint (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, prop_id, pspec);
    }
}

static void
gb_workspace_get_preferred_width (GtkWidget *widget,
                                  gint      *min_width,
                                  gint      *nat_width)
{
  GbWorkspace *self = (GbWorkspace *)widget;
  int i;

  g_assert (GB_IS_WORKSPACE (self));

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_width (child->widget, &child->min_width, &child->nat_width);
    }

  *min_width = self->children [GTK_POS_LEFT].min_width
             + self->children [GTK_POS_RIGHT].min_width
             + MAX (self->children [GTK_POS_TOP].min_width,
                    self->children [GTK_POS_BOTTOM].min_width);
  *nat_width = self->children [GTK_POS_LEFT].nat_width
             + self->children [GTK_POS_RIGHT].nat_width
             + MAX (self->children [GTK_POS_TOP].nat_width,
                    self->children [GTK_POS_BOTTOM].nat_width);
}

static void
gb_workspace_get_preferred_height (GtkWidget *widget,
                                   gint      *min_height,
                                   gint      *nat_height)
{
  GbWorkspace *self = (GbWorkspace *)widget;
  int i;

  g_assert (GB_IS_WORKSPACE (self));

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      if (gtk_widget_get_visible (child->widget))
        gtk_widget_get_preferred_height (child->widget, &child->min_height, &child->nat_height);
    }

  *min_height = MAX (MAX (self->children [GTK_POS_LEFT].min_height,
                          self->children [GTK_POS_RIGHT].min_height),
                     (self->children [GTK_POS_BOTTOM].position +
                      self->children [GTK_POS_TOP].min_height));

  *nat_height = MAX (MAX (self->children [GTK_POS_LEFT].nat_height,
                          self->children [GTK_POS_RIGHT].nat_height),
                     (self->children [GTK_POS_BOTTOM].position +
                      self->children [GTK_POS_TOP].nat_height));
}

static GtkSizeRequestMode
gb_workspace_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static GtkAdjustment *
gb_workspace_create_adjustment (GbWorkspace *self)
{
  GtkAdjustment *adj;

  g_assert (GB_IS_WORKSPACE (self));

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
gb_workspace_drag_begin_cb (GbWorkspace   *self,
                            gdouble        x,
                            gdouble        y,
                            GtkGesturePan *pan)
{
  GbWorkspaceChild *left;
  GbWorkspaceChild *right;
  GbWorkspaceChild *bottom;
  GdkEventSequence *sequence;
  const GdkEvent *event;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));

  left = &self->children [GTK_POS_LEFT];
  right = &self->children [GTK_POS_RIGHT];
  bottom = &self->children [GTK_POS_BOTTOM];

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (pan));
  event = gtk_gesture_get_last_event (GTK_GESTURE (pan), sequence);

  if (event->any.window == left->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_HORIZONTAL);
      self->drag_child = left;
    }
  else if (event->any.window == right->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_HORIZONTAL);
      self->drag_child = right;
    }
  else if (event->any.window == bottom->handle)
    {
      gtk_gesture_pan_set_orientation (pan, GTK_ORIENTATION_VERTICAL);
      self->drag_child = bottom;
    }
  else
    {
      gtk_gesture_set_state (GTK_GESTURE (pan), GTK_EVENT_SEQUENCE_DENIED);
      self->drag_child = NULL;
      return;
    }

  self->drag_position = MAX (self->drag_child->position, MIN_POSITION);
  gtk_gesture_set_state (GTK_GESTURE (pan), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_container_child_notify (GTK_CONTAINER (self), self->drag_child->widget, "position");
}

static void
gb_workspace_drag_end_cb (GbWorkspace   *self,
                          gdouble        x,
                          gdouble        y,
                          GtkGesturePan *pan)
{
  GdkEventSequence *sequence;
  GtkEventSequenceState state;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));

  if (self->drag_child == NULL)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (pan));
  state = gtk_gesture_get_sequence_state (GTK_GESTURE (pan), sequence);
  if (state == GTK_EVENT_SEQUENCE_DENIED)
    {
      self->drag_child = NULL;
      return;
    }

  if (self->drag_child->position < MIN_POSITION)
    {
      gtk_container_child_set (GTK_CONTAINER (self), self->drag_child->widget,
                               "reveal", FALSE,
                               NULL);
      self->drag_child->restore_position = self->drag_position;
    }

  gtk_container_child_notify (GTK_CONTAINER (self), self->drag_child->widget, "position");

  self->drag_child = NULL;
  self->drag_position = 0;
}

static void
gb_workspace_pan_cb (GbWorkspace     *self,
                     GtkPanDirection  direction,
                     gdouble          offset,
                     GtkGesturePan   *pan)
{
  GtkAllocation alloc;
  gint target_position = 0;
  gint center_min_width;
  gint left_max;
  gint right_max;
  gint bottom_max;

  g_assert (GB_IS_WORKSPACE (self));
  g_assert (GTK_IS_GESTURE_PAN (pan));
  g_assert (self->drag_child != NULL);

  /*
   * NOTE: This is trickier than it looks, so I choose to be
   *       very verbose. Feel free to clean it up.
   */

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  switch (direction)
    {
    case GTK_PAN_DIRECTION_LEFT:
      if (self->drag_child->type == GTK_POS_LEFT)
        target_position = self->drag_position - offset;
      else if (self->drag_child->type == GTK_POS_RIGHT)
        target_position = self->drag_position + offset;
      break;

    case GTK_PAN_DIRECTION_RIGHT:
      if (self->drag_child->type == GTK_POS_LEFT)
        target_position = self->drag_position + offset;
      else if (self->drag_child->type == GTK_POS_RIGHT)
        target_position = self->drag_position - offset;
      break;

    case GTK_PAN_DIRECTION_UP:
      if (self->drag_child->type == GTK_POS_BOTTOM)
        target_position = self->drag_position + offset;
      break;

    case GTK_PAN_DIRECTION_DOWN:
      if (self->drag_child->type == GTK_POS_BOTTOM)
        target_position = self->drag_position - offset;
      break;

    default:
      g_assert_not_reached ();
    }

  center_min_width = MAX (self->children [GTK_POS_BOTTOM].min_width,
                          self->children [GTK_POS_TOP].min_width);
  left_max = alloc.width - self->children [GTK_POS_RIGHT].alloc.width - center_min_width;
  right_max = alloc.width - self->children [GTK_POS_LEFT].position - center_min_width;
  bottom_max = alloc.height - self->children [GTK_POS_TOP].min_height;

  switch (self->drag_child->type)
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

  self->drag_child->position = MAX (0, target_position);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static GtkGesture *
gb_workspace_create_pan_gesture (GbWorkspace    *self,
                                 GtkOrientation  orientation)
{
  GtkGesture *gesture;

  g_assert (GB_IS_WORKSPACE (self));

  gesture = gtk_gesture_pan_new (GTK_WIDGET (self), orientation);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);

  g_signal_connect_object (gesture,
                           "drag-begin",
                           G_CALLBACK (gb_workspace_drag_begin_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (gesture,
                           "drag-end",
                           G_CALLBACK (gb_workspace_drag_end_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (gesture,
                           "pan",
                           G_CALLBACK (gb_workspace_pan_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return gesture;
}

static void
gb_workspace_realize (GtkWidget *widget)
{
  GbWorkspace *self = (GbWorkspace *)widget;

  g_assert (GB_IS_WORKSPACE (self));

  GTK_WIDGET_CLASS (gb_workspace_parent_class)->realize (widget);

  gb_workspace_create_handle_window (self, GTK_POS_LEFT);
  gb_workspace_create_handle_window (self, GTK_POS_RIGHT);
  gb_workspace_create_handle_window (self, GTK_POS_BOTTOM);
}

static void
gb_workspace_unrealize (GtkWidget *widget)
{
  GbWorkspace *self = (GbWorkspace *)widget;

  g_assert (GB_IS_WORKSPACE (self));

  gb_workspace_destroy_handle_window (self, GTK_POS_LEFT);
  gb_workspace_destroy_handle_window (self, GTK_POS_RIGHT);
  gb_workspace_destroy_handle_window (self, GTK_POS_BOTTOM);

  GTK_WIDGET_CLASS (gb_workspace_parent_class)->unrealize (widget);
}

static void
gb_workspace_map (GtkWidget *widget)
{
  GbWorkspace *self = (GbWorkspace *)widget;
  int i;

  g_assert (GB_IS_WORKSPACE (self));

  GTK_WIDGET_CLASS (gb_workspace_parent_class)->map (widget);

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      if (child->handle != NULL)
        gdk_window_show (child->handle);
    }
}

static void
gb_workspace_unmap (GtkWidget *widget)
{
  GbWorkspace *self = (GbWorkspace *)widget;
  int i;

  g_assert (GB_IS_WORKSPACE (self));

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      if (child->handle != NULL)
        gdk_window_hide (child->handle);
    }

  GTK_WIDGET_CLASS (gb_workspace_parent_class)->unmap (widget);
}

static GObject *
gb_workspace_get_internal_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 const gchar  *childname)
{
  GbWorkspace *self = (GbWorkspace *)buildable;

  g_assert (GB_IS_WORKSPACE (self));

  /*
   * Override default get_internal_child to handle RTL vs LTR.
   */
  if (ide_str_equal0 (childname, "left_pane"))
    return G_OBJECT (gb_workspace_get_left_pane (self));
  else if (ide_str_equal0 (childname, "right_pane"))
    return G_OBJECT (gb_workspace_get_right_pane (self));

  return gb_workspace_parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
gb_workspace_grab_focus (GtkWidget *widget)
{
  GbWorkspace *self = (GbWorkspace *)widget;

  gtk_widget_grab_focus (self->children [GTK_POS_TOP].widget);
}

static void
gb_workspace_finalize (GObject *object)
{
  GbWorkspace *self = (GbWorkspace *)object;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (self->children); i++)
    {
      GbWorkspaceChild *child = &self->children [i];

      ide_clear_weak_pointer (&child->animation);
      g_clear_object (&child->adjustment);
    }

  g_clear_object (&self->pan_gesture);

  G_OBJECT_CLASS (gb_workspace_parent_class)->finalize (object);
}

static void
gb_workspace_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkspace *self = GB_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_LEFT_PANE:
      g_value_set_object (value, gb_workspace_get_left_pane (self));
      break;

    case PROP_RIGHT_PANE:
      g_value_set_object (value, gb_workspace_get_right_pane (self));
      break;

    case PROP_BOTTOM_PANE:
      g_value_set_object (value, gb_workspace_get_bottom_pane (self));
      break;

    case PROP_CONTENT_PANE:
      g_value_set_object (value, gb_workspace_get_content_pane (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_set_property (GObject      *object,
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
  gb_workspace_parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->get_internal_child = gb_workspace_get_internal_child;
}

static void
gb_workspace_class_init (GbWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkOverlayClass *overlay_class = GTK_OVERLAY_CLASS (klass);

  object_class->finalize = gb_workspace_finalize;
  object_class->get_property = gb_workspace_get_property;
  object_class->set_property = gb_workspace_set_property;

  widget_class->get_preferred_height = gb_workspace_get_preferred_height;
  widget_class->get_preferred_width = gb_workspace_get_preferred_width;
  widget_class->get_request_mode = gb_workspace_get_request_mode;
  widget_class->map = gb_workspace_map;
  widget_class->unmap = gb_workspace_unmap;
  widget_class->realize = gb_workspace_realize;
  widget_class->unrealize = gb_workspace_unrealize;
  widget_class->size_allocate = gb_workspace_size_allocate;
  widget_class->grab_focus = gb_workspace_grab_focus;

  container_class->get_child_property = gb_workspace_get_child_property;
  container_class->set_child_property = gb_workspace_set_child_property;

  overlay_class->get_child_position = gb_workspace_get_child_position;

  gParamSpecs [PROP_LEFT_PANE] =
    g_param_spec_object ("left-pane",
                         _("Left Pane"),
                         _("The left workspace pane."),
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_RIGHT_PANE] =
    g_param_spec_object ("right-pane",
                         _("Right Pane"),
                         _("The right workspace pane."),
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_BOTTOM_PANE] =
    g_param_spec_object ("bottom-pane",
                         _("Bottom Pane"),
                         _("The bottom workspace pane."),
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_CONTENT_PANE] =
    g_param_spec_object ("content-pane",
                         _("Content Pane"),
                         _("The content workspace pane."),
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gChildParamSpecs [CHILD_PROP_POSITION] =
    g_param_spec_uint ("position",
                       _("Position"),
                       _("The position of the pane relative to it's edge."),
                       0, G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gtk_container_class_install_child_property (container_class, CHILD_PROP_POSITION,
                                              gChildParamSpecs [CHILD_PROP_POSITION]);

  gChildParamSpecs [CHILD_PROP_REVEAL] =
    g_param_spec_boolean ("reveal",
                          _("Reveal"),
                          _("If the pane should be revealed."),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  gtk_container_class_install_child_property (container_class, CHILD_PROP_REVEAL,
                                              gChildParamSpecs [CHILD_PROP_REVEAL]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-workspace.ui");

  gtk_widget_class_bind_template_child_full (widget_class, "bottom_pane", TRUE,
                                             G_STRUCT_OFFSET (GbWorkspace, children[GTK_POS_BOTTOM].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "content_pane", TRUE,
                                             G_STRUCT_OFFSET (GbWorkspace, children[GTK_POS_TOP].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "left_pane", TRUE,
                                             G_STRUCT_OFFSET (GbWorkspace, children[GTK_POS_LEFT].widget));
  gtk_widget_class_bind_template_child_full (widget_class, "right_pane", TRUE,
                                             G_STRUCT_OFFSET (GbWorkspace, children[GTK_POS_RIGHT].widget));
}

static void
gb_workspace_init (GbWorkspace *self)
{
  self->children [GTK_POS_LEFT].type = GTK_POS_LEFT;
  self->children [GTK_POS_LEFT].reveal = TRUE;
  self->children [GTK_POS_LEFT].position = 250;
  self->children [GTK_POS_LEFT].adjustment = gb_workspace_create_adjustment (self);
  self->children [GTK_POS_LEFT].cursor_type = GDK_SB_H_DOUBLE_ARROW;

  self->children [GTK_POS_RIGHT].type = GTK_POS_RIGHT;
  self->children [GTK_POS_RIGHT].reveal = TRUE;
  self->children [GTK_POS_RIGHT].position = 250;
  self->children [GTK_POS_RIGHT].adjustment = gb_workspace_create_adjustment (self);
  self->children [GTK_POS_RIGHT].cursor_type = GDK_SB_H_DOUBLE_ARROW;

  self->children [GTK_POS_BOTTOM].type = GTK_POS_BOTTOM;
  self->children [GTK_POS_BOTTOM].reveal = TRUE;
  self->children [GTK_POS_BOTTOM].position = 150;
  self->children [GTK_POS_BOTTOM].adjustment = gb_workspace_create_adjustment (self);
  self->children [GTK_POS_BOTTOM].cursor_type = GDK_SB_V_DOUBLE_ARROW;

  self->children [GTK_POS_TOP].type = GTK_POS_TOP;
  self->children [GTK_POS_TOP].reveal = TRUE;
  self->children [GTK_POS_TOP].adjustment = gb_workspace_create_adjustment (self);

  self->pan_gesture = gb_workspace_create_pan_gesture (self, GTK_ORIENTATION_HORIZONTAL);

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gb_workspace_new (void)
{
  return g_object_new (GB_TYPE_WORKSPACE, NULL);
}

GtkWidget *
gb_workspace_get_left_pane (GbWorkspace *self)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  if (gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_DIR_RTL)
    return self->children [GTK_POS_RIGHT].widget;
  else
    return self->children [GTK_POS_LEFT].widget;
}

GtkWidget *
gb_workspace_get_right_pane (GbWorkspace *self)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  if (gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_DIR_RTL)
    return self->children [GTK_POS_LEFT].widget;
  else
    return self->children [GTK_POS_RIGHT].widget;
}

GtkWidget *
gb_workspace_get_bottom_pane (GbWorkspace *self)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  return self->children [GTK_POS_BOTTOM].widget;
}

GtkWidget *
gb_workspace_get_content_pane (GbWorkspace *self)
{
  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  return self->children [GTK_POS_TOP].widget;
}
