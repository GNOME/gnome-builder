/* gstyle-color-widget.c
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gstyle-color-widget"

#include <cairo.h>
#include <string.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include "gstyle-color-widget-actions.h"
#include "gstyle-css-provider.h"
#include "gstyle-palette-widget.h"
#include "gstyle-private.h"

#include "gstyle-color-widget.h"

struct _GstyleColorWidget
{
  GtkBin                         parent_instance;

  GstyleCssProvider             *default_provider;

  GtkLabel                      *label;
  GstyleColor                   *color;
  GstyleColor                   *filtered_color;
  GstyleColorKind                fallback_name_kind;
  GstyleColorFilterFunc          filter_func;
  gpointer                       filter_user_data;

  GtkBorder                      cached_margin;
  GtkBorder                      cached_border;

  cairo_pattern_t               *checkered_pattern;

  GtkTargetList                 *target_list;
  GstyleColorWidget             *dnd_color_widget;
  GtkWidget                     *dnd_window;
  gboolean                       is_on_drag;

  GtkGesture                    *drag_gesture;
  GtkGesture                    *multipress_gesture;

  GstylePaletteWidgetViewMode    container_view_mode;
  GstyleColorWidgetDndLockFlags  dnd_lock : 4;
  guint                          is_in_palette_widget : 1;
  guint                          is_name_visible : 1;
  guint                          is_fallback_name_visible : 1;
};

G_DEFINE_TYPE (GstyleColorWidget, gstyle_color_widget, GTK_TYPE_BIN)

#define GSTYLE_COLOR_WIDGET_DROP_BORDER_PERCENT 0.20
#define GSTYLE_COLOR_WIDGET_DRAG_ICON_OPACITY 0.8

enum {
  PROP_0,
  PROP_COLOR,
  PROP_DND_LOCK,
  PROP_NAME_VISIBLE,
  PROP_FALLBACK_NAME_KIND,
  PROP_FALLBACK_NAME_VISIBLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
is_in_drop_zone (GstyleColorWidget *self,
                 gint               x,
                 gint               y)
{
  GtkAllocation alloc;
  gint start_limit;
  gint stop_limit;
  gint dest_ref;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  if (self->is_in_palette_widget)
    {
      if (self->container_view_mode == GSTYLE_PALETTE_WIDGET_VIEW_MODE_LIST)
        {
          start_limit = alloc.height * GSTYLE_COLOR_WIDGET_DROP_BORDER_PERCENT;
          stop_limit = alloc.height * (1.0 - GSTYLE_COLOR_WIDGET_DROP_BORDER_PERCENT);
          dest_ref = y;
        }
      else
        {
          start_limit = alloc.width * GSTYLE_COLOR_WIDGET_DROP_BORDER_PERCENT;
          stop_limit = alloc.width * (1.0 - GSTYLE_COLOR_WIDGET_DROP_BORDER_PERCENT);
          dest_ref = x;
        }
    }
  else
    {
      /* dest_ref doesn't matter here, we just need to allow for the whole widget size */
      start_limit = 0;
      stop_limit = alloc.width;
      dest_ref = x;
    }

  return (start_limit < dest_ref && dest_ref < stop_limit);
}

static GstylePaletteWidgetDndLockFlags
get_palette_widget_dnd_lock (GstyleColorWidget *self)
{
  GtkWidget *palette_widget;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  palette_widget = gtk_widget_get_ancestor (GTK_WIDGET (self), GSTYLE_TYPE_PALETTE_WIDGET);
  if (palette_widget != NULL)
    return gstyle_palette_widget_get_dnd_lock (GSTYLE_PALETTE_WIDGET (palette_widget));
  else
    return GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_NONE;
}

static void
gstyle_color_widget_drag_gesture_update (GtkGestureDrag    *gesture,
                                         gdouble            offset_x,
                                         gdouble            offset_y,
                                         GstyleColorWidget *self)
{
  GdkDragContext *context;
  GdkEventSequence *sequence;
  const GdkEvent *event;
  gdouble start_x, start_y;
  GtkAllocation allocation;
  GstylePaletteWidgetDndLockFlags dnd_lock;
  GtkWidget *container;
  GdkDragAction drag_action;
  gint button;

  g_assert (GTK_IS_GESTURE (gesture));
  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  dnd_lock = get_palette_widget_dnd_lock (self);
  if ((dnd_lock & GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG) != 0)
    return;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  if (!gtk_drag_check_threshold (GTK_WIDGET (self), 0, 0, offset_x, offset_y) ||
      button != GDK_BUTTON_PRIMARY)
    return;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  self->dnd_color_widget = gstyle_color_widget_copy (self);

  if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
    gstyle_color_widget_set_color (self->dnd_color_widget, self->filtered_color);

  self->dnd_window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_widget_set_size_request (self->dnd_window, allocation.width, allocation.height);
  gtk_window_set_screen (GTK_WINDOW (self->dnd_window), gtk_widget_get_screen (GTK_WIDGET (self)));

  gtk_container_add (GTK_CONTAINER (self->dnd_window), GTK_WIDGET (self->dnd_color_widget));
  gtk_widget_set_opacity (self->dnd_window, GSTYLE_COLOR_WIDGET_DRAG_ICON_OPACITY);

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture), &start_x, &start_y);
  event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

  container = gtk_widget_get_ancestor (GTK_WIDGET (self), GSTYLE_TYPE_PALETTE_WIDGET);
  if (container != NULL && GSTYLE_IS_PALETTE_WIDGET (container))
    drag_action = (GDK_ACTION_MOVE | GDK_ACTION_COPY);
  else
    drag_action = GDK_ACTION_COPY;

  context = gtk_drag_begin_with_coordinates (GTK_WIDGET (self),
                                             self->target_list,
                                             drag_action,
                                             button,
                                             (GdkEvent*)event,
                                             start_x, start_y);

  gtk_drag_set_icon_widget (context, self->dnd_window, 0, 0);
}

static gboolean
gstyle_color_widget_on_drag_motion (GtkWidget      *widget,
                                    GdkDragContext *context,
                                    gint            x,
                                    gint            y,
                                    guint           time)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GstylePaletteWidgetDndLockFlags dnd_lock;
  GdkAtom target;
  GdkDragAction drag_action;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  target = gtk_drag_dest_find_target (widget, context, NULL);
  dnd_lock = get_palette_widget_dnd_lock (self);
  if ((dnd_lock & GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DRAG) != 0)
    {
      gdk_drag_status (context, 0, time);
      return FALSE;
    }

  if ((target == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET") ||
       target == gdk_atom_intern_static_string ("application/x-color") ||
       gtk_targets_include_text (&target, 1)) &&
      (dnd_lock & GSTYLE_PALETTE_WIDGET_DND_LOCK_FLAGS_DROP) == 0 &&
      is_in_drop_zone (self, x, y))
    {
      gtk_drag_highlight (widget);

      drag_action = gdk_drag_context_get_actions (context);
      if (drag_action & GDK_ACTION_COPY)
        {
          gdk_drag_status (context, GDK_ACTION_COPY, time);
          return TRUE;
        }
    }

  gdk_drag_status (context, 0, time);
  return FALSE;
}

static void
gstyle_color_widget_on_drag_leave (GtkWidget      *widget,
                                   GdkDragContext *context,
                                   guint           time)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (widget));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  gtk_drag_unhighlight (widget);
}

static gboolean
gstyle_color_widget_on_drag_drop (GtkWidget        *widget,
                                  GdkDragContext   *context,
                                  gint              x,
                                  gint              y,
                                  guint             time)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GdkAtom target;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if ((target == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET") ||
       target == gdk_atom_intern_static_string ("application/x-color") ||
       gtk_targets_include_text (&target, 1)) &&
      is_in_drop_zone (self, x, y))
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }

  return FALSE;
}

static void
gstyle_color_widget_on_drag_data_delete (GtkWidget      *widget,
                                         GdkDragContext *context)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GActionGroup *group;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "gstyle-color-widget-menu");
  if (group != NULL)
    g_action_group_activate_action (group, "remove", NULL);
}

static void
dnd_color_fill (GstyleColorWidget *self,
                GstyleColor       *src_color,
                GstyleColor       *dst_color)
{
  const gchar *name;
  GdkRGBA src_rgba;
  GdkRGBA dst_rgba;

  g_assert (GSTYLE_COLOR_WIDGET (self));
  g_assert (GSTYLE_COLOR (src_color));
  g_assert (GSTYLE_COLOR (dst_color));

  gstyle_color_fill_rgba (src_color, &src_rgba);
  gstyle_color_fill_rgba (dst_color, &dst_rgba);
  if (!(self->dnd_lock & GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_COLOR))
    {
      dst_rgba.red = src_rgba.red;
      dst_rgba.green = src_rgba.green;
      dst_rgba.blue = src_rgba.blue;
    }

  if (!(self->dnd_lock & GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA))
    dst_rgba.alpha = src_rgba.alpha;

  gstyle_color_set_rgba (self->color, &dst_rgba);

  if (!(self->dnd_lock & GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND))
    gstyle_color_set_kind (dst_color, gstyle_color_get_kind (src_color));

  if (!(self->dnd_lock & GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME))
  {
    name = gstyle_color_get_name (src_color);
    gstyle_color_set_name (dst_color, name);
  }
}

static void
gstyle_color_widget_on_drag_data_received (GtkWidget        *widget,
                                           GdkDragContext   *context,
                                           gint              x,
                                           gint              y,
                                           GtkSelectionData *data,
                                           guint             info,
                                           guint             time)
{
  GstyleColorWidget *self = GSTYLE_COLOR_WIDGET (widget);
  GtkWidget *ancestor;
  GstylePalette *selected_palette;
  GstyleColor * const *src_color;
  g_autofree gchar *color_string = NULL;
  GstyleColorKind kind;
  GdkAtom target;
  guint16 *data_rgba;
  GdkRGBA rgba;
  gint len;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  target = gtk_selection_data_get_target (data);
  if (target == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET"))
    {
      src_color = (void*)gtk_selection_data_get_data (data);
      if (*src_color != self->color)
        {
          dnd_color_fill (self, *src_color, self->color);
          if (NULL != (ancestor = gtk_widget_get_ancestor (widget, GSTYLE_TYPE_PALETTE_WIDGET)) &&
              NULL != (selected_palette = gstyle_palette_widget_get_selected_palette (GSTYLE_PALETTE_WIDGET (ancestor))))
            gstyle_palette_set_changed (selected_palette, TRUE);
        }

      gtk_drag_finish (context, TRUE, FALSE, time);

      return;
    }
  else if (target == gdk_atom_intern_static_string ("application/x-color"))
    {
      len = gtk_selection_data_get_length (data);
      if (len < 0 )
        goto failed;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

      data_rgba = (guint16 *)gtk_selection_data_get_data (data);

#pragma GCC diagnostic pop

      rgba.red = data_rgba[0] / 65535.;
      rgba.green = data_rgba[1] / 65535.;
      rgba.blue = data_rgba[2] / 65535.;
      rgba.alpha = data_rgba[3] / 65535.;

      gstyle_color_set_rgba (self->color, &rgba);
      gtk_drag_finish (context, TRUE, FALSE, time);

      return;
    }
  else if (gtk_targets_include_text (&target, 1))
    {
      color_string = (gchar *)gtk_selection_data_get_text (data);
      if (!gstyle_str_empty0 (color_string))
        {
          if (!gstyle_color_parse_color_string (color_string, &rgba, &kind))
            goto failed;

          gstyle_color_set_rgba (self->color, &rgba);
          gtk_drag_finish (context, TRUE, FALSE, time);
        }
    }

failed:
  gtk_drag_finish (context, FALSE, FALSE, time);
}

static void
contextual_popover_closed_cb (GstyleColorWidget *self,
                              GtkWidget         *popover)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GTK_IS_WIDGET (popover));

  gtk_widget_destroy (popover);
}

static void
popover_button_rename_clicked_cb (GstyleColorWidget *self,
                                  GdkEvent          *event,
                                  GtkButton         *button)
{
  GActionGroup *group;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "gstyle-color-widget-menu");
  if (group != NULL)
    g_action_group_activate_action (group, "rename", NULL);
}

static void
popover_button_remove_clicked_cb (GstyleColorWidget *self,
                                  GdkEvent          *event,
                                  GtkButton         *button)
{
  GActionGroup *group;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GTK_IS_BUTTON (button));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "gstyle-color-widget-menu");
  if (group != NULL)
    g_action_group_activate_action (group, "remove", NULL);
}

/* The multi-press gesture used by the flowbox to select a child
 * forbid us to use dnd so we need to catch it here and select yourself
 * the child
 */
static void
gstyle_color_widget_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                                guint                 n_press,
                                                gdouble               x,
                                                gdouble               y,
                                                GstyleColorWidget    *self)
{
  GtkWidget *container;
  GtkWidget *child;
  GtkWidget *popover;
  GtkBuilder *builder;
  GtkWidget *button_rename;
  GtkWidget *button_remove;
  GtkWidget *ancestor;
  gint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  child = gtk_widget_get_parent (GTK_WIDGET (self));
  if (child != NULL && button == GDK_BUTTON_PRIMARY)
    {
      if (GTK_IS_LIST_BOX_ROW (child))
        {
          container = gtk_widget_get_parent (GTK_WIDGET (child));
          if (container != NULL && GTK_IS_LIST_BOX (container))
            {
              gtk_list_box_select_row (GTK_LIST_BOX (container), GTK_LIST_BOX_ROW (child));
              gtk_widget_grab_focus (GTK_WIDGET (self));
              if (n_press == 2)
                g_signal_emit_by_name (container, "row-activated", child);
            }
        }
      else if (GTK_IS_FLOW_BOX_CHILD (child))
        {
          container = gtk_widget_get_parent (GTK_WIDGET (child));
          if (container != NULL && GTK_IS_FLOW_BOX (container))
            {
              gtk_flow_box_select_child (GTK_FLOW_BOX (container), GTK_FLOW_BOX_CHILD (child));
              gtk_widget_grab_focus (GTK_WIDGET (self));
              if (n_press == 2)
                g_signal_emit_by_name (container, "child-activated", child);
            }
        }
    }

  if (button == GDK_BUTTON_SECONDARY)
    {
      ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), GSTYLE_TYPE_PALETTE_WIDGET);
      if (ancestor != NULL)
        {
          builder = gtk_builder_new_from_resource ("/org/gnome/libgstyle/ui/gstyle-color-widget.ui");
          popover = GTK_WIDGET (gtk_builder_get_object (builder, "popover"));
          button_rename = GTK_WIDGET (gtk_builder_get_object (builder, "button_rename"));
          g_signal_connect_object (button_rename, "button-release-event",
                                   G_CALLBACK (popover_button_rename_clicked_cb), self, G_CONNECT_SWAPPED);

          button_remove = GTK_WIDGET (gtk_builder_get_object (builder, "button_remove"));
          g_signal_connect_object (button_remove, "button-release-event",
                                   G_CALLBACK (popover_button_remove_clicked_cb), self, G_CONNECT_SWAPPED);

          gtk_popover_set_relative_to (GTK_POPOVER (popover), GTK_WIDGET (self));
          g_signal_connect_swapped (popover, "closed", G_CALLBACK (contextual_popover_closed_cb), self);
          gtk_popover_popup (GTK_POPOVER (popover));
          g_object_unref (builder);
        }
    }
}

static void
gstyle_color_widget_on_drag_data_get (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      GtkSelectionData *data,
                                      guint             info,
                                      guint             time)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GdkAtom target = gtk_selection_data_get_target (data);
  GstyleColor *color;
  guint16 data_rgba[4];
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
    color = self->filtered_color;
  else
    color = self->color;

  if (target == gdk_atom_intern_static_string ("GSTYLE_COLOR_WIDGET"))
    gtk_selection_data_set (data, target, 8, (void*)&color, sizeof (gpointer));
  else if (target == gdk_atom_intern_static_string ("application/x-color"))
    {
      gstyle_color_fill_rgba (color, &rgba);
      data_rgba[0] = (guint16) (rgba.red * 65535);
      data_rgba[1] = (guint16) (rgba.green * 65535);
      data_rgba[2] = (guint16) (rgba.blue * 65535);
      data_rgba[3] = (guint16) (rgba.alpha * 65535);

      gtk_selection_data_set (data, target, 16, (void*)&data_rgba, 8);
    }
  else if (gtk_targets_include_text (&target, 1))
    {
      g_autofree gchar *name = NULL;

      name = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_ORIGINAL);
      if (name == NULL)
        name = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_HEX6);

      gtk_selection_data_set_text (data, name, -1);
    }
}

static void
gstyle_color_widget_on_drag_end (GtkWidget      *widget,
                                 GdkDragContext *context)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  gtk_widget_destroy (self->dnd_window);
  self->dnd_window = NULL;
  self->dnd_color_widget = NULL;
  self->is_on_drag = FALSE;
}

static gboolean
gstyle_color_widget_on_drag_failed (GtkWidget      *widget,
                                    GdkDragContext *context,
                                    GtkDragResult   result)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (widget));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  return FALSE;
}

static void
update_border_and_margin (GstyleColorWidget *self)
{
  GtkStyleContext *style_context;
  GtkStateFlags state;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state = gtk_style_context_get_state (style_context);

  gtk_style_context_get_margin (style_context, state, &self->cached_margin);
  gtk_style_context_get_border (style_context, state, &self->cached_border);
}

static void
gstyle_color_widget_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GtkAllocation child_allocation;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  gtk_widget_set_allocation (widget, allocation);

  if (self->label && gtk_widget_get_visible (GTK_WIDGET (self->label)))
  {
    child_allocation.x = 0;
    child_allocation.y = 0;
    child_allocation.width = allocation->width;
    child_allocation.height = allocation->height;

    gtk_widget_size_allocate (GTK_WIDGET (self->label), &child_allocation);
  }

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);
}

static void
gstyle_color_widget_get_preferred_width (GtkWidget *widget,
                                         gint      *min_width,
                                         gint      *nat_width)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GtkWidget *child;
  gint spacing;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  *min_width = 1;
  *nat_width = 1;

  update_border_and_margin (self);

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_width (child, min_width, nat_width);

  spacing = self->cached_border.left +
            self->cached_border.right +
            self->cached_margin.left +
            self->cached_margin.right;

  *min_width += spacing;
  *nat_width += spacing;
}

static void
gstyle_color_widget_get_preferred_height (GtkWidget *widget,
                                          gint      *min_height,
                                          gint      *nat_height)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GtkWidget *child;
  gint spacing;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  *min_height = 1;
  *nat_height = 1;

  update_border_and_margin (self);

  child = gtk_bin_get_child (GTK_BIN (self));
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_height (child, min_height, nat_height);

  spacing = self->cached_border.top +
            self->cached_border.bottom +
            self->cached_margin.top +
            self->cached_margin.bottom;

  *min_height += spacing;
  *nat_height += spacing;
}

static gboolean
gstyle_color_widget_draw (GtkWidget *widget,
                          cairo_t   *cr)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;
  GtkStyleContext *style_context;
  GdkRectangle margin_box;
  GdkRectangle border_box;
  cairo_matrix_t matrix;
  GdkRGBA bg_color = {0};
  gint radius;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (cr != NULL);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_widget_get_allocation (widget, &margin_box);
  margin_box.x = margin_box.y = 0;

  gstyle_utils_get_rect_resized_box (margin_box, &margin_box, &self->cached_margin);
  gstyle_utils_get_rect_resized_box (margin_box, &border_box, &self->cached_border);
  cairo_save (cr);

  if (self->color != NULL)
    {
      gtk_style_context_get (style_context,
                            gtk_style_context_get_state (style_context),
                            "border-radius", &radius,
                            NULL);

      if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
        gstyle_color_fill_rgba (self->filtered_color, &bg_color);
      else
        gstyle_color_fill_rgba (self->color, &bg_color);

      cairo_new_path (cr);
      draw_cairo_round_box (cr, border_box, radius, radius, radius, radius);
    }
  else
    cairo_rectangle (cr, border_box.x, border_box.y, border_box.width, border_box.height);

  cairo_clip_preserve (cr);

  cairo_set_source_rgb (cr, 0.20, 0.20, 0.20);
  cairo_paint (cr);
  cairo_set_source_rgb (cr, 0.80, 0.80, 0.80);

  cairo_matrix_init_scale (&matrix, 0.1, 0.1);
  cairo_matrix_translate (&matrix, -border_box.x, -border_box.y);
  cairo_pattern_set_matrix (self->checkered_pattern, &matrix);
  cairo_mask (cr, self->checkered_pattern);

  if (self->color != NULL)
    {
      gdk_cairo_set_source_rgba (cr, &bg_color);
      cairo_fill (cr);
    }
  else
    gtk_render_background (style_context, cr, border_box.x, border_box.y, border_box.width, border_box.height);

  cairo_restore (cr);
  gtk_render_frame (gtk_widget_get_style_context (widget), cr,
                    margin_box.x, margin_box.y, margin_box.width, margin_box.height);

  return GTK_WIDGET_CLASS (gstyle_color_widget_parent_class)->draw (widget, cr);
}

static void
update_label_visibility (GstyleColorWidget *self)
{
  const gchar *color_name;
  g_autofree gchar *fallback_name = NULL;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  if (self->color == NULL)
    {
      if (gtk_widget_is_visible (GTK_WIDGET (self->label)))
        gtk_widget_hide (GTK_WIDGET (self->label));

      return;
    }

  if (self->is_name_visible)
    {
      if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
        color_name = gstyle_color_get_name (self->filtered_color);
      else
        color_name = gstyle_color_get_name (self->color);

      if (color_name != NULL)
        {
          gtk_label_set_text (self->label, color_name);
          if (!gtk_widget_is_visible (GTK_WIDGET (self->label)))
            gtk_widget_show (GTK_WIDGET (self->label));

          return;
        }
    }

  if (self->is_fallback_name_visible)
    {
      if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
        fallback_name = gstyle_color_to_string (self->filtered_color, self->fallback_name_kind);
      else
        fallback_name = gstyle_color_to_string (self->color, self->fallback_name_kind);

      gtk_label_set_text (self->label, fallback_name);
      if (!gtk_widget_is_visible (GTK_WIDGET (self->label)))
        gtk_widget_show (GTK_WIDGET (self->label));
    }
  else
    gtk_widget_hide (GTK_WIDGET (self->label));
}

static void
match_label_color (GstyleColorWidget *self,
                   GstyleColor       *color)
{
  PangoLayout *layout;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  GdkRGBA rgba;
  GdkRGBA dst_rgba;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GSTYLE_IS_COLOR (color));

  layout = gtk_label_get_layout (self->label);
  attr_list = pango_layout_get_attributes (layout);
  if (attr_list == NULL)
    {
      attr_list = pango_attr_list_new ();
      gtk_label_set_attributes (self->label, attr_list);
      pango_attr_list_unref (attr_list);
    }

  gstyle_color_fill_rgba (color, &rgba);
  gstyle_utils_get_contrasted_rgba (rgba, &dst_rgba);
  attr = pango_attr_foreground_new (dst_rgba.red * 0xffff, dst_rgba.green * 0xffff, dst_rgba.blue * 0xffff);
  pango_attr_list_change (attr_list, attr);
  attr = pango_attr_background_new (rgba.red * 0xffff, rgba.green * 0xffff, rgba.blue * 0xffff);
  pango_attr_list_change (attr_list, attr);
}

static void
gstyle_color_widget_rgba_notify_cb (GstyleColorWidget *self,
                                    GParamSpec        *pspec,
                                    GstyleColor       *color)
{
  GdkRGBA rgba;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (GSTYLE_IS_COLOR (color));

  if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
    {
      gstyle_color_fill_rgba (color, &rgba);
      self->filter_func (&rgba, &rgba, self->filter_user_data);
      gstyle_color_set_rgba (self->filtered_color, &rgba);
    }

  update_label_visibility (self);

  if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
    match_label_color (self, self->filtered_color);
  else
    match_label_color (self, color);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gstyle_color_widget_name_notify_cb (GstyleColorWidget *self,
                                    GParamSpec        *pspec,
                                    GstyleColor       *color)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (G_IS_PARAM_SPEC (pspec));
  g_assert (GSTYLE_IS_COLOR (color));

  update_label_visibility (self);
}

static void
gstyle_color_widget_disconnect_color (GstyleColorWidget *self)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GSTYLE_IS_COLOR (self->color));

  g_signal_handlers_disconnect_by_func (self->color,
                                        G_CALLBACK (gstyle_color_widget_rgba_notify_cb),
                                        self);

 g_signal_handlers_disconnect_by_func (self->color,
                                       G_CALLBACK (gstyle_color_widget_name_notify_cb),
                                       self);
}

/**
 * gstyle_color_widget_copy:
 * @self: a #GstyleColorWidget
 *
 * Copy the given ##GstyleColorWidget.
 * Notice that the underlaying #GstyleColor is shared
 * between the two widgets (the copy increase the ref count)
 *
 * Returns: (transfer full): a new #GstyleColorWidget.
 *
 */
GstyleColorWidget *
gstyle_color_widget_copy (GstyleColorWidget *self)
{
  GstyleColorWidget *color_widget;
  GstyleColor *color;
  gboolean name_visible;
  GstyleColorKind fallback_name_kind;
  gboolean fallback_name_visible;

  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), NULL);

  color = gstyle_color_widget_get_color (self);
  name_visible = gstyle_color_widget_get_name_visible (self);
  fallback_name_visible = gstyle_color_widget_get_name_visible (self);
  fallback_name_kind = gstyle_color_widget_get_fallback_name_kind (self);

  color_widget = gstyle_color_widget_new_with_color (color);
  gstyle_color_widget_set_name_visible (color_widget, name_visible);
  gstyle_color_widget_set_name_visible (color_widget, fallback_name_visible);
  gstyle_color_widget_set_fallback_name_kind (color_widget, fallback_name_kind);

  return color_widget;
}

/**
 * gstyle_color_widget_get_filter_func: (skip):
 * @self: a #GstyleColorPlane
 *
 * Get a pointer to the current filter function or %NULL
 * if no filter is actually set.
 *
 * Returns: (nullable): A GstyleColorFilterFunc function pointer.
 *
 */
GstyleColorFilterFunc
gstyle_color_widget_get_filter_func (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), NULL);

  return self->filter_func;
}

/**
 * gstyle_color_widget_set_filter_func:
 * @self: a #GstyleColorPlane
 * @filter_func: (scope notified) (nullable): A GstyleColorFilterFunc filter function or
 *   %NULL to unset the current filter. In this case, user_data is ignored
 * @user_data: (closure) (nullable): user data to pass when calling the filter function
 *
 * Set a filter to be used to change the color drawn.
 *
 */
void
gstyle_color_widget_set_filter_func (GstyleColorWidget    *self,
                                    GstyleColorFilterFunc  filter_func,
                                    gpointer               user_data)
{
  GdkRGBA rgba;
  GdkRGBA filtered_rgba;

  g_return_if_fail (GSTYLE_IS_COLOR_WIDGET (self));

  self->filter_func = filter_func;
  self->filter_user_data = (filter_func == NULL) ? NULL : user_data;

  if (filter_func == NULL)
    {
      g_clear_object (&self->filtered_color);
      match_label_color (self, self->color);
      update_label_visibility (self);
    }
  else
    {
      gstyle_color_fill_rgba (self->color, &rgba);
      self->filter_func (&rgba, &filtered_rgba, self->filter_user_data);

      g_clear_object (&self->filtered_color);
      self->filtered_color = gstyle_color_copy (self->color);
      gstyle_color_set_rgba (self->filtered_color, &filtered_rgba);

      if (!gdk_rgba_equal (&rgba, &filtered_rgba))
        {
          update_label_visibility (self);
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
        }

      match_label_color (self, self->filtered_color);
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * gstyle_color_widget_set_color:
 * @self: a #GstyleColorWidget
 * @color: (nullable): a #GstyleColor or %NULL
 *
 * Set the #GstyleColor for the #GstyleColorWidget.
 *
 */
void
gstyle_color_widget_set_color (GstyleColorWidget *self,
                               GstyleColor       *color)
{
  GdkRGBA rgba;

  g_return_if_fail (GSTYLE_IS_COLOR_WIDGET (self));
  g_return_if_fail (GSTYLE_IS_COLOR (color) || color == NULL);

  if (self->color != color)
    {
      if (self->color != NULL)
        {
          gstyle_color_widget_disconnect_color (self);
          g_clear_object (&self->color);
        }

      if (color != NULL)
        {
          self->color = g_object_ref (color);
          if (self->filter_func != NULL)
            {
              gstyle_color_fill_rgba (color, &rgba);
              self->filter_func (&rgba, &rgba, self->filter_user_data);

              g_clear_object (&self->filtered_color);
              self->filtered_color = gstyle_color_copy (color);
              gstyle_color_set_rgba (self->filtered_color, &rgba);
            }

          g_signal_connect_object (self->color,
                                   "notify::rgba",
                                   G_CALLBACK (gstyle_color_widget_rgba_notify_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_signal_connect_object (self->color,
                                   "notify::name",
                                   G_CALLBACK (gstyle_color_widget_name_notify_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

          if (self->filter_func != NULL && GSTYLE_IS_COLOR (self->filtered_color))
            match_label_color (self, self->filtered_color);
          else
            match_label_color (self, color);
        }

      update_label_visibility (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
    }
}

/**
 * gstyle_color_widget_get_name_visible:
 * @self: a #GstyleColorWidget
 *
 * Get the visibily of the #GstyleColorWidget name.
 *
 * Returns: %TRUE if the name should be visible, %FALSE otherwise.
 *
 */
gboolean
gstyle_color_widget_get_name_visible (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), FALSE);

  return self->is_name_visible;
}

/**
 * gstyle_color_widget_get_fallback_name_visible:
 * @self: a #GstyleColorWidget
 *
 * Get the visibily of the #GstyleColorWidget fallback name.
 *
 * Returns: %TRUE if the name should be visible, %FALSE otherwise.
 *
 */
gboolean
gstyle_color_widget_get_fallback_name_visible (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), FALSE);

  return self->is_fallback_name_visible;
}

/**
 * gstyle_color_widget_get_fallback_name_kind:
 * @self: a #GstyleColorWidget
 *
 * Get the kind of the #GstyleColorWidget fallback name.
 *
 * Returns: the #GstyleColorKind used for the fallback name.
 *
 */
GstyleColorKind
gstyle_color_widget_get_fallback_name_kind (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), GSTYLE_COLOR_KIND_UNKNOW);

  return self->fallback_name_kind;
}

/**
 * gstyle_color_widget_set_name_visible:
 * @self: a #GstyleColorWidget
 * @visible: name visibility
 *
 * Set the visibily of the #GstyleColorWidget name.
 *
 */
void
gstyle_color_widget_set_name_visible (GstyleColorWidget *self,
                                      gboolean           visible)
{
  g_return_if_fail (GSTYLE_IS_COLOR_WIDGET (self));

  self->is_name_visible = visible;
  update_label_visibility (self);
}

/**
 * gstyle_color_widget_set_fallback_name_visible:
 * @self: a #GstyleColorWidget
 * @visible: fallback name visibility
 *
 * Set the visibily of the #GstyleColorWidget fallback name.
 *
 */
void
gstyle_color_widget_set_fallback_name_visible (GstyleColorWidget *self,
                                               gboolean           visible)
{
  g_return_if_fail (GSTYLE_IS_COLOR_WIDGET (self));

  self->is_fallback_name_visible = visible;
  update_label_visibility (self);
}

/**
 * gstyle_color_widget_set_fallback_name_kind:
 * @self: a #GstyleColorWidget
 * @kind: a #GstyleColorKind
 *
 * Set the kind of the #GstyleColorWidget fallback name.
 *
 */
void
gstyle_color_widget_set_fallback_name_kind (GstyleColorWidget *self,
                                            GstyleColorKind    kind)
{
  g_return_if_fail (GSTYLE_IS_COLOR_WIDGET (self));

  self->fallback_name_kind = kind;
  update_label_visibility (self);
}

/**
 * gstyle_color_widget_get_color:
 * @self: a #GstyleColorWidget
 *
 * Get the #GstyleColor of the #GstyleColorWidget.
 *
 * Returns: (transfer none): The current affected #GstyleColor.
 *
 */
GstyleColor *
gstyle_color_widget_get_color (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), NULL);

  return self->color;
}

/**
 * gstyle_color_widget_get_filtered_color:
 * @self: a #GstyleColorWidget
 *
 * If a #GstyleColorFilterFunc is set, Get the filtered #GstyleColor
 * of the #GstyleColorWidget, otherwise, get the regular #GstyleColor.
 *
 * Returns: (transfer none): The affected #GstyleColor or filtered one.
 *
 */
GstyleColor *
gstyle_color_widget_get_filtered_color (GstyleColorWidget *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_WIDGET (self), NULL);

  if (self->filter_func != NULL)
    return self->filtered_color;
  else
    return self->color;
}

/**
 * gstyle_color_widget_new:
 *
 * Returns: A new #GstyleColorWidget.
 *
 */
GstyleColorWidget *
gstyle_color_widget_new (void)
{
  return g_object_new (GSTYLE_TYPE_COLOR_WIDGET, NULL);
}

/**
 * gstyle_color_widget_new_with_color:
 * @color: a #GstyleColor
 *
 * Returns: A new #GstyleColorWidget.with @color affected.
 *
 */
GstyleColorWidget *
gstyle_color_widget_new_with_color (GstyleColor *color)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR (color), NULL);

  return g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                       "color", color,
                       NULL);
}

static void
update_container_parent_informations (GstyleColorWidget *self)
{
  GtkWidget *parent;
  GtkWidget *grand_parent;
  GtkWidget *container;
  GstylePaletteWidget *palette_widget;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));
  if (GTK_IS_LIST_BOX_ROW (parent) || GTK_IS_FLOW_BOX_CHILD (parent))
    {
      grand_parent = gtk_widget_get_parent (GTK_WIDGET (parent));
      if (grand_parent != NULL && g_str_has_prefix (gtk_widget_get_name (grand_parent), "palette"))
        {
          self->is_in_palette_widget = TRUE;
          container = gtk_widget_get_ancestor (grand_parent, GSTYLE_TYPE_PALETTE_WIDGET);
          if (container != NULL && GSTYLE_IS_PALETTE_WIDGET (container))
            {
              palette_widget = GSTYLE_PALETTE_WIDGET (container);
              self->container_view_mode = gstyle_palette_widget_get_view_mode (GSTYLE_PALETTE_WIDGET (palette_widget));

              return;
            }
        }
    }

  self->is_in_palette_widget = FALSE;
}

static void
gstyle_color_widget_hierarchy_changed (GtkWidget *widget,
                                       GtkWidget *previous_toplevel)
{
  GstyleColorWidget *self = (GstyleColorWidget *)widget;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  update_container_parent_informations (self);
}

static gboolean
gstyle_color_widget_key_pressed_cb (GstyleColorWidget *self,
                                    GdkEventKey       *event)
{
  GtkWidget *ancestor;
  GActionGroup *group;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (event != NULL);

  if (event->type != GDK_KEY_PRESS)
    return GDK_EVENT_PROPAGATE;

  ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), GSTYLE_TYPE_PALETTE_WIDGET);
  if (event->keyval == GDK_KEY_F2 && ancestor != NULL)
    {
      group = gtk_widget_get_action_group (GTK_WIDGET (self), "gstyle-color-widget-menu");
      if (group != NULL)
        g_action_group_activate_action (group, "rename", NULL);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gstyle_color_widget_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes = {0};

  g_assert (GTK_IS_WIDGET (widget));

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_set_realized (widget, TRUE);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_BUTTON_MOTION_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_POINTER_MOTION_MASK
                        | GDK_ENTER_NOTIFY_MASK
                        | GDK_LEAVE_NOTIFY_MASK;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL);
  gtk_widget_set_window (widget, g_object_ref (window));
  gtk_widget_register_window (widget, window);
  gdk_window_show (window);
}

static void
gstyle_color_widget_finalize (GObject *object)
{
  GstyleColorWidget *self = GSTYLE_COLOR_WIDGET (object);

  if (self->color != NULL)
    gstyle_color_widget_disconnect_color (self);

  g_clear_object (&self->multipress_gesture);
  g_clear_object (&self->drag_gesture);

  g_clear_object (&self->dnd_window);
  g_clear_object (&self->color);
  g_clear_object (&self->filtered_color);
  g_clear_object (&self->default_provider);

  gstyle_clear_pointer (&self->checkered_pattern, cairo_pattern_destroy);
  gstyle_clear_pointer (&self->target_list, gtk_target_list_unref);

  G_OBJECT_CLASS (gstyle_color_widget_parent_class)->finalize (object);
}

static void
gstyle_color_widget_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GstyleColorWidget *self = GSTYLE_COLOR_WIDGET (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      g_value_set_object (value, gstyle_color_widget_get_color (self));
      break;

    case PROP_DND_LOCK:
      g_value_set_flags (value, self->dnd_lock);
      break;

    case PROP_NAME_VISIBLE:
      g_value_set_boolean (value, gstyle_color_widget_get_name_visible (self));
      break;

    case PROP_FALLBACK_NAME_KIND:
      g_value_set_enum (value, gstyle_color_widget_get_fallback_name_kind (self));
      break;

    case PROP_FALLBACK_NAME_VISIBLE:
      g_value_set_boolean (value, gstyle_color_widget_get_fallback_name_visible (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_widget_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GstyleColorWidget *self = GSTYLE_COLOR_WIDGET (object);

  switch (prop_id)
    {
    case PROP_COLOR:
      gstyle_color_widget_set_color (self, g_value_get_object (value));
      break;

    case PROP_DND_LOCK:
      self->dnd_lock = g_value_get_flags (value);
      break;

    case PROP_NAME_VISIBLE:
      gstyle_color_widget_set_name_visible (self, g_value_get_boolean (value));
      break;

    case PROP_FALLBACK_NAME_KIND:
      gstyle_color_widget_set_fallback_name_kind (self, g_value_get_enum (value));
      break;

    case PROP_FALLBACK_NAME_VISIBLE:
      gstyle_color_widget_set_fallback_name_visible (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_widget_class_init (GstyleColorWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = (GtkContainerClass*) klass;

  object_class->finalize = gstyle_color_widget_finalize;
  object_class->get_property = gstyle_color_widget_get_property;
  object_class->set_property = gstyle_color_widget_set_property;

  widget_class->size_allocate = gstyle_color_widget_size_allocate;
  widget_class->realize = gstyle_color_widget_realize;
  widget_class->get_preferred_width = gstyle_color_widget_get_preferred_width;
  widget_class->get_preferred_height = gstyle_color_widget_get_preferred_height;
  widget_class->hierarchy_changed = gstyle_color_widget_hierarchy_changed;
  widget_class->draw = gstyle_color_widget_draw;

  widget_class->drag_end = gstyle_color_widget_on_drag_end;
  widget_class->drag_failed = gstyle_color_widget_on_drag_failed;
  widget_class->drag_data_get = gstyle_color_widget_on_drag_data_get;
  widget_class->drag_data_delete = gstyle_color_widget_on_drag_data_delete;

  widget_class->drag_motion = gstyle_color_widget_on_drag_motion;
  widget_class->drag_leave = gstyle_color_widget_on_drag_leave;
  widget_class->drag_drop = gstyle_color_widget_on_drag_drop;
  widget_class->drag_data_received = gstyle_color_widget_on_drag_data_received;

  properties[PROP_COLOR] =
    g_param_spec_object ("color",
                         "color",
                         "A GstyleColor to use name and color from",
                         GSTYLE_TYPE_COLOR,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME_VISIBLE] =
    g_param_spec_boolean ("name-visible",
                          "name-visible",
                          "set the color name visibility",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_FALLBACK_NAME_VISIBLE] =
    g_param_spec_boolean ("fallback-name-visible",
                          "fallback-name-visible",
                          "set the fallback name visibility",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_FALLBACK_NAME_KIND] =
    g_param_spec_enum ("fallback-name-kind",
                       "fallback-name-kind",
                       "if there's no name, the fallback kind displayed",
                       GSTYLE_TYPE_COLOR_KIND,
                       GSTYLE_COLOR_KIND_ORIGINAL,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_DND_LOCK] =
    g_param_spec_flags ("dnd-lock",
                        "dnd-lock",
                        "Dnd lockability",
                        GSTYLE_TYPE_COLOR_WIDGET_DND_LOCK_FLAGS,
                        GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NONE,
                        (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_container_class_handle_border_width (container_class);
  gtk_widget_class_set_css_name (widget_class, "gstylecolorwidget");
}



static const GtkTargetEntry dnd_targets [] = {
  { (gchar *)"GSTYLE_COLOR_WIDGET", GTK_TARGET_SAME_APP, 0 },
  { (gchar *)"application/x-color", 0, 0 },
};

static void
gstyle_color_widget_init (GstyleColorWidget *self)
{
  GtkStyleContext *context;
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);

  self->label = GTK_LABEL (g_object_new (GTK_TYPE_LABEL,
                                         "ellipsize", PANGO_ELLIPSIZE_END,
                                         "visible", TRUE,
                                         "halign", GTK_ALIGN_CENTER,
                                         "valign", GTK_ALIGN_CENTER,
                                         NULL));

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->label));
  self->is_name_visible = TRUE;
  self->is_fallback_name_visible = TRUE;
  self->fallback_name_kind = GSTYLE_COLOR_KIND_RGB_HEX6;

  self->checkered_pattern = gstyle_utils_get_checkered_pattern ();

  gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));

  self->target_list = gtk_target_list_new (dnd_targets, G_N_ELEMENTS (dnd_targets));
  gtk_target_list_add_text_targets (self->target_list, 0);

  gtk_drag_dest_set (widget, 0, NULL, 0, GDK_ACTION_MOVE);
  gtk_drag_dest_set_target_list (widget, self->target_list);
  gtk_drag_dest_set_track_motion (GTK_WIDGET (self), TRUE);

  update_container_parent_informations (self);

  self->multipress_gesture = gtk_gesture_multi_press_new (widget);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->multipress_gesture),
                                              GTK_PHASE_BUBBLE);
  g_signal_connect (self->multipress_gesture, "pressed",
                    G_CALLBACK (gstyle_color_widget_multipress_gesture_pressed), widget);

  self->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (self));
  g_signal_connect (self->drag_gesture, "drag-update",
                    G_CALLBACK (gstyle_color_widget_drag_gesture_update), self);

  g_signal_connect_swapped (self, "key-press-event",
                            G_CALLBACK (gstyle_color_widget_key_pressed_cb),
                            self);

  gstyle_color_widget_actions_init (self);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
}

GType
gstyle_color_widget_dnd_lock_flags_get_type (void)
{
  static GType type_id;
  static const GFlagsValue values[] = {
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NONE,  "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NONE",  "none" },
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND,  "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND",  "kind" },
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME,  "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME",  "name" },
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA, "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA", "alpha" },
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_COLOR, "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_COLOR", "COLOR" },
    { GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALL,   "GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALL",   "all" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_flags_register_static ("GstyleColorWidgetDndLockFlags", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
