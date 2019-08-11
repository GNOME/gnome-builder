/*
 * Copyright 2011 Red Hat, Inc.
 * Copyright 2013 Ignacio Casal Quinteiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <math.h>

#include "ide-tagged-entry.h"

#define BUTTON_INTERNAL_SPACING 6

struct _IdeTaggedEntryTagPrivate {
  IdeTaggedEntry *entry;
  GdkWindow *window;
  PangoLayout *layout;

  gchar *label;
  gchar *style;
  gboolean has_close_button;

  cairo_surface_t *close_surface;
  GtkStateFlags last_button_state;
};

struct _IdeTaggedEntryPrivate {
  GList *tags;

  IdeTaggedEntryTag *in_child;
  gboolean in_child_button;
  gboolean in_child_active;
  gboolean in_child_button_active;
  gboolean button_visible;
};

enum {
  SIGNAL_TAG_CLICKED,
  SIGNAL_TAG_BUTTON_CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TAG_BUTTON_VISIBLE,
  NUM_PROPERTIES
};

enum {
  PROP_TAG_0,
  PROP_TAG_LABEL,
  PROP_TAG_HAS_CLOSE_BUTTON,
  PROP_TAG_STYLE,
  NUM_TAG_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTaggedEntry, ide_tagged_entry, GTK_TYPE_SEARCH_ENTRY)
G_DEFINE_TYPE_WITH_PRIVATE (IdeTaggedEntryTag, ide_tagged_entry_tag, G_TYPE_OBJECT)

static guint signals[LAST_SIGNAL] = { 0, };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static GParamSpec *tag_properties[NUM_TAG_PROPERTIES] = { NULL, };

static void ide_tagged_entry_get_text_area_size (GtkEntry *entry,
                                                gint *x,
                                                gint *y,
                                                gint *width,
                                                gint *height);
static gint ide_tagged_entry_tag_get_width (IdeTaggedEntryTag *tag,
                                           IdeTaggedEntry *entry);
static GtkStyleContext * ide_tagged_entry_tag_get_context (IdeTaggedEntryTag *tag,
                                                          IdeTaggedEntry *entry);

static void
ide_tagged_entry_tag_get_margin (IdeTaggedEntryTag *tag,
                                IdeTaggedEntry *entry,
                                GtkBorder *margin)
{
  GtkStyleContext *context;

  context = ide_tagged_entry_tag_get_context (tag, entry);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
  gtk_style_context_get_margin (context,
                                gtk_style_context_get_state (context),
                                margin);
  gtk_style_context_restore (context);
}

static void
ide_tagged_entry_tag_ensure_close_surface (IdeTaggedEntryTag *tag,
                                          GtkStyleContext *context)
{
  GtkIconInfo *info;
  GdkPixbuf *pixbuf;
  gint icon_size;
  gint scale_factor;

  if (tag->priv->close_surface != NULL)
    return;

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU,
                        &icon_size, NULL);
  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (tag->priv->entry));

  info = gtk_icon_theme_lookup_icon_for_scale (gtk_icon_theme_get_default (),
                                               "window-close-symbolic",
                                               icon_size, scale_factor,
                                               GTK_ICON_LOOKUP_GENERIC_FALLBACK);

  /* FIXME: we need a fallback icon in case the icon is not found */
  pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);
  tag->priv->close_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale_factor, tag->priv->window);

  g_object_unref (info);
  g_object_unref (pixbuf);
}

static gint
ide_tagged_entry_tag_panel_get_height (IdeTaggedEntryTag *tag,
                                      IdeTaggedEntry *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  gint height, req_height;
  GtkRequisition requisition;
  GtkAllocation allocation;
  GtkBorder margin;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_get_preferred_size (widget, &requisition, NULL);
  ide_tagged_entry_tag_get_margin (tag, entry, &margin);

  /* the tag panel height is the whole entry height, minus the tag margins */
  req_height = requisition.height - gtk_widget_get_margin_top (widget) - gtk_widget_get_margin_bottom (widget);
  height = MIN (req_height, allocation.height) - margin.top - margin.bottom;

  return height;
}

static void
ide_tagged_entry_tag_panel_get_position (IdeTaggedEntry *self,
                                        gint *x_out,
                                        gint *y_out)
{
  GtkWidget *widget = GTK_WIDGET (self);
  gint text_x, text_y, text_width, text_height, req_height;
  GtkAllocation allocation;
  GtkRequisition requisition;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_get_preferred_size (widget, &requisition, NULL);
  req_height = requisition.height - gtk_widget_get_margin_top (widget) - gtk_widget_get_margin_bottom (widget);

  ide_tagged_entry_get_text_area_size (GTK_ENTRY (self), &text_x, &text_y, &text_width, &text_height);

  /* allocate the panel immediately after the text area */
  if (x_out)
    *x_out = allocation.x + text_x + text_width;
  if (y_out)
    *y_out = allocation.y + (gint) floor ((allocation.height - req_height) / 2);
}

static gint
ide_tagged_entry_tag_panel_get_width (IdeTaggedEntry *self)
{
  IdeTaggedEntryTag *tag;
  gint width;
  GList *l;

  width = 0;

  for (l = self->priv->tags; l != NULL; l = l->next)
    {
      tag = l->data;
      width += ide_tagged_entry_tag_get_width (tag, self);
    }

  return width;
}

static void
ide_tagged_entry_tag_ensure_layout (IdeTaggedEntryTag *tag,
                                   IdeTaggedEntry *entry)
{
  if (tag->priv->layout != NULL)
    return;

  tag->priv->layout = pango_layout_new (gtk_widget_get_pango_context (GTK_WIDGET (entry)));
  pango_layout_set_text (tag->priv->layout, tag->priv->label, -1);
}

static GtkStateFlags
ide_tagged_entry_tag_get_state (IdeTaggedEntryTag *tag,
                               IdeTaggedEntry *entry)
{
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;

  if (entry->priv->in_child == tag)
    state |= GTK_STATE_FLAG_PRELIGHT;

  if (entry->priv->in_child_active)
    state |= GTK_STATE_FLAG_ACTIVE;

  return state;
}

static GtkStateFlags
ide_tagged_entry_tag_get_button_state (IdeTaggedEntryTag *tag,
                                      IdeTaggedEntry *entry)
{
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;

  if (entry->priv->in_child == tag)
    {
      if (entry->priv->in_child_button_active)
        state |= GTK_STATE_FLAG_ACTIVE;

      else if (entry->priv->in_child_button)
        state |= GTK_STATE_FLAG_PRELIGHT;
    }

  return state;
}

static GtkStyleContext *
ide_tagged_entry_tag_get_context (IdeTaggedEntryTag *tag,
                                 IdeTaggedEntry    *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkStyleContext *retval;
  GList *l, *list;

  retval = gtk_widget_get_style_context (widget);
  gtk_style_context_save (retval);

  list = gtk_style_context_list_classes (retval);
  for (l = list; l; l = l->next)
    gtk_style_context_remove_class (retval, l->data);
  g_list_free (list);
  gtk_style_context_add_class (retval, tag->priv->style);

  return retval;
}

static gint
ide_tagged_entry_tag_get_width (IdeTaggedEntryTag *tag,
                               IdeTaggedEntry *entry)
{
  GtkBorder button_padding, button_border, button_margin;
  GtkStyleContext *context;
  GtkStateFlags state;
  gint layout_width;
  gint button_width;
  gint scale_factor;

  ide_tagged_entry_tag_ensure_layout (tag, entry);
  pango_layout_get_pixel_size (tag->priv->layout, &layout_width, NULL);

  context = ide_tagged_entry_tag_get_context (tag, entry);
  state = ide_tagged_entry_tag_get_state (tag, entry);

  gtk_style_context_set_state (context, state);
  gtk_style_context_get_padding (context,
                                 gtk_style_context_get_state (context),
                                 &button_padding);
  gtk_style_context_get_border (context,
                                gtk_style_context_get_state (context),
                                &button_border);
  gtk_style_context_get_margin (context,
                                gtk_style_context_get_state (context),
                                &button_margin);

  ide_tagged_entry_tag_ensure_close_surface (tag, context);

  gtk_style_context_restore (context);

  button_width = 0;
  if (entry->priv->button_visible && tag->priv->has_close_button)
    {
      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (entry));
      button_width = cairo_image_surface_get_width (tag->priv->close_surface) / scale_factor +
        BUTTON_INTERNAL_SPACING;
    }

  return layout_width + button_padding.left + button_padding.right +
    button_border.left + button_border.right +
    button_margin.left + button_margin.right +
    button_width;
}

static void
ide_tagged_entry_tag_get_size (IdeTaggedEntryTag *tag,
                              IdeTaggedEntry *entry,
                              gint *width_out,
                              gint *height_out)
{
  gint width, panel_height;

  width = ide_tagged_entry_tag_get_width (tag, entry);
  panel_height = ide_tagged_entry_tag_panel_get_height (tag, entry);

  if (width_out)
    *width_out = width;
  if (height_out)
    *height_out = panel_height;
}

static void
ide_tagged_entry_tag_get_relative_allocations (IdeTaggedEntryTag *tag,
                                              IdeTaggedEntry *entry,
                                              GtkStyleContext *context,
                                              GtkAllocation *background_allocation_out,
                                              GtkAllocation *layout_allocation_out,
                                              GtkAllocation *button_allocation_out)
{
  GtkAllocation background_allocation, layout_allocation, button_allocation;
  gint width, height, x, y, pix_width, pix_height;
  gint layout_width, layout_height;
  gint scale_factor;
  GtkBorder padding, border;
  GtkStateFlags state;

  width = gdk_window_get_width (tag->priv->window);
  height = gdk_window_get_height (tag->priv->window);
  scale_factor = gdk_window_get_scale_factor (tag->priv->window);

  state = ide_tagged_entry_tag_get_state (tag, entry);
  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  gtk_style_context_get_margin (context,
                                gtk_style_context_get_state (context),
                                &padding);
  gtk_style_context_restore (context);

  width -= padding.left + padding.right;
  height -= padding.top + padding.bottom;
  x = padding.left;
  y = padding.top;

  background_allocation.x = x;
  background_allocation.y = y;
  background_allocation.width = width;
  background_allocation.height = height;

  layout_allocation = button_allocation = background_allocation;

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  gtk_style_context_get_padding (context,
                                 gtk_style_context_get_state (context),
                                 &padding);
  gtk_style_context_get_border (context,
                                gtk_style_context_get_state (context),
                                &border);
  gtk_style_context_restore (context);

  ide_tagged_entry_tag_ensure_layout (tag, entry);
  pango_layout_get_pixel_size (tag->priv->layout, &layout_width, &layout_height);

  layout_allocation.x += border.left + padding.left;
  layout_allocation.y += (layout_allocation.height - layout_height) / 2;

  if (entry->priv->button_visible && tag->priv->has_close_button)
    {
      pix_width = cairo_image_surface_get_width (tag->priv->close_surface) / scale_factor;
      pix_height = cairo_image_surface_get_height (tag->priv->close_surface) / scale_factor;
    }
  else
    {
      pix_width = 0;
      pix_height = 0;
    }

  button_allocation.x += width - pix_width - border.right - padding.right;
  button_allocation.y += (height - pix_height) / 2;
  button_allocation.width = pix_width;
  button_allocation.height = pix_height;

  if (background_allocation_out)
    *background_allocation_out = background_allocation;
  if (layout_allocation_out)
    *layout_allocation_out = layout_allocation;
  if (button_allocation_out)
    *button_allocation_out = button_allocation;
}

static gboolean
ide_tagged_entry_tag_event_is_button (IdeTaggedEntryTag *tag,
                                     IdeTaggedEntry *entry,
                                     gdouble event_x,
                                     gdouble event_y)
{
  GtkAllocation button_allocation;
  GtkStyleContext *context;

  if (!entry->priv->button_visible || !tag->priv->has_close_button)
    return FALSE;

  context = ide_tagged_entry_tag_get_context (tag, entry);
  ide_tagged_entry_tag_get_relative_allocations (tag, entry, context, NULL, NULL, &button_allocation);

  gtk_style_context_restore (context);

  /* see if the event falls into the button allocation */
  if ((event_x >= button_allocation.x &&
       event_x <= button_allocation.x + button_allocation.width) &&
      (event_y >= button_allocation.y &&
       event_y <= button_allocation.y + button_allocation.height))
    return TRUE;

  return FALSE;
}

gboolean
ide_tagged_entry_tag_get_area (IdeTaggedEntryTag      *tag,
                              cairo_rectangle_int_t *rect)
{
  GtkStyleContext *context;
  GtkAllocation background_allocation;
  int window_x, window_y;
  GtkAllocation alloc;

  g_return_val_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  gdk_window_get_position (tag->priv->window, &window_x, &window_y);
  gtk_widget_get_allocation (GTK_WIDGET (tag->priv->entry), &alloc);
  context = ide_tagged_entry_tag_get_context (tag, tag->priv->entry);
  ide_tagged_entry_tag_get_relative_allocations (tag, tag->priv->entry, context,
                                                &background_allocation,
                                                NULL, NULL);
  gtk_style_context_restore (context);

  rect->x = window_x - alloc.x + background_allocation.x;
  rect->y = window_y - alloc.y + background_allocation.y;
  rect->width = background_allocation.width;
  rect->height = background_allocation.height;

  return TRUE;
}

static void
ide_tagged_entry_tag_draw (IdeTaggedEntryTag *tag,
                          cairo_t *cr,
                          IdeTaggedEntry *entry)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkAllocation background_allocation, layout_allocation, button_allocation;

  context = ide_tagged_entry_tag_get_context (tag, entry);
  ide_tagged_entry_tag_get_relative_allocations (tag, entry, context,
                                                &background_allocation,
                                                &layout_allocation,
                                                &button_allocation);

  cairo_save (cr);
  gtk_cairo_transform_to_window (cr, GTK_WIDGET (entry), tag->priv->window);

  gtk_style_context_save (context);

  state = ide_tagged_entry_tag_get_state (tag, entry);
  gtk_style_context_set_state (context, state);
  gtk_render_background (context, cr,
                         background_allocation.x, background_allocation.y,
                         background_allocation.width, background_allocation.height);
  gtk_render_frame (context, cr,
                    background_allocation.x, background_allocation.y,
                    background_allocation.width, background_allocation.height);

  gtk_render_layout (context, cr,
                     layout_allocation.x, layout_allocation.y,
                     tag->priv->layout);

  gtk_style_context_restore (context);

  if (!entry->priv->button_visible || !tag->priv->has_close_button)
    goto done;

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);
  state = ide_tagged_entry_tag_get_button_state (tag, entry);
  gtk_style_context_set_state (context, state);

  /* if the state changed since last time we draw the pixbuf,
   * clear and redraw it.
   */
  if (state != tag->priv->last_button_state)
    {
      g_clear_pointer (&tag->priv->close_surface, cairo_surface_destroy);
      ide_tagged_entry_tag_ensure_close_surface (tag, context);

      tag->priv->last_button_state = state;
    }

  gtk_render_background (context, cr,
                         button_allocation.x, button_allocation.y,
                         button_allocation.width, button_allocation.height);
  gtk_render_frame (context, cr,
                         button_allocation.x, button_allocation.y,
                         button_allocation.width, button_allocation.height);

  gtk_render_icon_surface (context, cr,
                           tag->priv->close_surface,
                           button_allocation.x, button_allocation.y);

done:
  gtk_style_context_restore (context);

  cairo_restore (cr);
}

static void
ide_tagged_entry_tag_unrealize (IdeTaggedEntryTag *tag)
{
  if (tag->priv->window == NULL)
    return;

  gdk_window_set_user_data (tag->priv->window, NULL);
  gdk_window_destroy (tag->priv->window);
  tag->priv->window = NULL;
}

static void
ide_tagged_entry_tag_realize (IdeTaggedEntryTag *tag,
                             IdeTaggedEntry *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint tag_width, tag_height;

  if (tag->priv->window != NULL)
    return;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_BUTTON_PRESS_MASK
    | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK
    | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

  ide_tagged_entry_tag_get_size (tag, entry, &tag_width, &tag_height);
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = tag_width;
  attributes.height = tag_height;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  tag->priv->window = gdk_window_new (gtk_widget_get_window (widget),
                                &attributes, attributes_mask);
  gdk_window_set_user_data (tag->priv->window, widget);
}

static gboolean
ide_tagged_entry_draw (GtkWidget *widget,
                      cairo_t *cr)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;
  GList *l;

  GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->draw (widget, cr);

  for (l = self->priv->tags; l != NULL; l = l->next)
    {
      tag = l->data;
      ide_tagged_entry_tag_draw (tag, cr, self);
    }

  return FALSE;
}

static void
ide_tagged_entry_map (GtkWidget *widget)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;
  GList *l;

  if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
    {
      GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->map (widget);

      for (l = self->priv->tags; l != NULL; l = l->next)
        {
          tag = l->data;
          gdk_window_show (tag->priv->window);
        }
    }
}

static void
ide_tagged_entry_unmap (GtkWidget *widget)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;
  GList *l;

  if (gtk_widget_get_mapped (widget))
    {
      for (l = self->priv->tags; l != NULL; l = l->next)
        {
          tag = l->data;
          gdk_window_hide (tag->priv->window);
        }

      GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->unmap (widget);
    }
}

static void
ide_tagged_entry_realize (GtkWidget *widget)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;
  GList *l;

  GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->realize (widget);

  for (l = self->priv->tags; l != NULL; l = l->next)
    {
      tag = l->data;
      ide_tagged_entry_tag_realize (tag, self);
    }
}

static void
ide_tagged_entry_unrealize (GtkWidget *widget)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;
  GList *l;

  GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->unrealize (widget);

  for (l = self->priv->tags; l != NULL; l = l->next)
    {
      tag = l->data;
      ide_tagged_entry_tag_unrealize (tag);
    }
}

static void
ide_tagged_entry_get_text_area_size (GtkEntry *entry,
                                    gint *x,
                                    gint *y,
                                    gint *width,
                                    gint *height)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (entry);
  gint tag_panel_width;

  GTK_ENTRY_CLASS (ide_tagged_entry_parent_class)->get_text_area_size (entry, x, y, width, height);

  tag_panel_width = ide_tagged_entry_tag_panel_get_width (self);

  if (width)
    *width -= tag_panel_width;
}

static void
ide_tagged_entry_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  gint x, y, width, height;
  IdeTaggedEntryTag *tag;
  GList *l;

  gtk_widget_set_allocation (widget, allocation);
  GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      ide_tagged_entry_tag_panel_get_position (self, &x, &y);

      for (l = self->priv->tags; l != NULL; l = l->next)
        {
          GtkBorder margin;

          tag = l->data;
          ide_tagged_entry_tag_get_size (tag, self, &width, &height);
          ide_tagged_entry_tag_get_margin (tag, self, &margin);
          gdk_window_move_resize (tag->priv->window, x, y + margin.top, width, height);

          x += width;
        }

      gtk_widget_queue_draw (widget);
    }
}

static void
ide_tagged_entry_get_preferred_width (GtkWidget *widget,
                                     gint *minimum,
                                     gint *natural)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  gint tag_panel_width;

  GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->get_preferred_width (widget, minimum, natural);

  tag_panel_width = ide_tagged_entry_tag_panel_get_width (self);

  if (minimum)
    *minimum += tag_panel_width;
  if (natural)
    *natural += tag_panel_width;
}

static void
ide_tagged_entry_finalize (GObject *obj)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (obj);

  if (self->priv->tags != NULL)
    {
      g_list_free_full (self->priv->tags, g_object_unref);
      self->priv->tags = NULL;
    }

  G_OBJECT_CLASS (ide_tagged_entry_parent_class)->finalize (obj);
}

static IdeTaggedEntryTag *
ide_tagged_entry_find_tag_by_window (IdeTaggedEntry *self,
                                    GdkWindow *window)
{
  IdeTaggedEntryTag *tag = NULL, *elem;
  GList *l;

  for (l = self->priv->tags; l != NULL; l = l->next)
    {
      elem = l->data;
      if (elem->priv->window == window)
        {
          tag = elem;
          break;
        }
    }

  return tag;
}

static gint
ide_tagged_entry_enter_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;

  tag = ide_tagged_entry_find_tag_by_window (self, event->window);

  if (tag != NULL)
    {
      self->priv->in_child = tag;
      gtk_widget_queue_draw (widget);
    }

  return GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->enter_notify_event (widget, event);
}

static gint
ide_tagged_entry_leave_notify (GtkWidget        *widget,
                              GdkEventCrossing *event)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);

  if (self->priv->in_child != NULL)
    {
      self->priv->in_child = NULL;
      gtk_widget_queue_draw (widget);
    }

  return GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->leave_notify_event (widget, event);
}

static gint
ide_tagged_entry_motion_notify (GtkWidget      *widget,
                               GdkEventMotion *event)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;

  tag = ide_tagged_entry_find_tag_by_window (self, event->window);

  if (tag != NULL)
    {
      gdk_event_request_motions (event);

      self->priv->in_child = tag;
      self->priv->in_child_button = ide_tagged_entry_tag_event_is_button (tag, self, event->x, event->y);
      gtk_widget_queue_draw (widget);

      return FALSE;
    }

  return GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->motion_notify_event (widget, event);
}

static gboolean
ide_tagged_entry_button_release_event (GtkWidget *widget,
                                      GdkEventButton *event)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;

  tag = ide_tagged_entry_find_tag_by_window (self, event->window);

  if (tag != NULL)
    {
      self->priv->in_child_active = FALSE;

      if (ide_tagged_entry_tag_event_is_button (tag, self, event->x, event->y))
        {
          self->priv->in_child_button_active = FALSE;
          g_signal_emit (self, signals[SIGNAL_TAG_BUTTON_CLICKED], 0, tag);
        }
      else
        {
          g_signal_emit (self, signals[SIGNAL_TAG_CLICKED], 0, tag);
        }

      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->button_release_event (widget, event);
}

static gboolean
ide_tagged_entry_button_press_event (GtkWidget *widget,
                                    GdkEventButton *event)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (widget);
  IdeTaggedEntryTag *tag;

  tag = ide_tagged_entry_find_tag_by_window (self, event->window);

  if (tag != NULL)
    {
      if (ide_tagged_entry_tag_event_is_button (tag, self, event->x, event->y))
        self->priv->in_child_button_active = TRUE;
      else
        self->priv->in_child_active = TRUE;

      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (ide_tagged_entry_parent_class)->button_press_event (widget, event);
}

static void
ide_tagged_entry_init (IdeTaggedEntry *self)
{
  self->priv = ide_tagged_entry_get_instance_private (self);
  self->priv->button_visible = TRUE;
}

static void
ide_tagged_entry_get_property (GObject      *object,
                              guint         property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (object);

  switch (property_id)
    {
      case PROP_TAG_BUTTON_VISIBLE:
        g_value_set_boolean (value, ide_tagged_entry_get_tag_button_visible (self));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ide_tagged_entry_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTaggedEntry *self = IDE_TAGGED_ENTRY (object);

  switch (property_id)
    {
      case PROP_TAG_BUTTON_VISIBLE:
        ide_tagged_entry_set_tag_button_visible (self, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ide_tagged_entry_class_init (IdeTaggedEntryClass *klass)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
  GtkEntryClass *eclass = GTK_ENTRY_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = ide_tagged_entry_finalize;
  oclass->set_property = ide_tagged_entry_set_property;
  oclass->get_property = ide_tagged_entry_get_property;

  wclass->realize = ide_tagged_entry_realize;
  wclass->unrealize = ide_tagged_entry_unrealize;
  wclass->map = ide_tagged_entry_map;
  wclass->unmap = ide_tagged_entry_unmap;
  wclass->size_allocate = ide_tagged_entry_size_allocate;
  wclass->get_preferred_width = ide_tagged_entry_get_preferred_width;
  wclass->draw = ide_tagged_entry_draw;
  wclass->enter_notify_event = ide_tagged_entry_enter_notify;
  wclass->leave_notify_event = ide_tagged_entry_leave_notify;
  wclass->motion_notify_event = ide_tagged_entry_motion_notify;
  wclass->button_press_event = ide_tagged_entry_button_press_event;
  wclass->button_release_event = ide_tagged_entry_button_release_event;

  eclass->get_text_area_size = ide_tagged_entry_get_text_area_size;

  signals[SIGNAL_TAG_CLICKED] =
    g_signal_new ("tag-clicked",
                  IDE_TYPE_TAGGED_ENTRY,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, IDE_TYPE_TAGGED_ENTRY_TAG);
  signals[SIGNAL_TAG_BUTTON_CLICKED] =
    g_signal_new ("tag-button-clicked",
                  IDE_TYPE_TAGGED_ENTRY,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, IDE_TYPE_TAGGED_ENTRY_TAG);

  properties[PROP_TAG_BUTTON_VISIBLE] =
    g_param_spec_boolean ("tag-close-visible", "Tag close icon visibility",
                          "Whether the close button should be shown in tags.", TRUE,
                          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
ide_tagged_entry_tag_init (IdeTaggedEntryTag *self)
{
  self->priv = ide_tagged_entry_tag_get_instance_private (self);
  self->priv->last_button_state = GTK_STATE_FLAG_NORMAL;
}

static void
ide_tagged_entry_tag_finalize (GObject *obj)
{
  IdeTaggedEntryTag *tag = IDE_TAGGED_ENTRY_TAG (obj);
  IdeTaggedEntryTagPrivate *priv = tag->priv;

  if (priv->window != NULL)
    ide_tagged_entry_tag_unrealize (tag);

  g_clear_object (&priv->layout);
  g_clear_pointer (&priv->close_surface, cairo_surface_destroy);
  g_free (priv->label);
  g_free (priv->style);

  G_OBJECT_CLASS (ide_tagged_entry_tag_parent_class)->finalize (obj);
}

static void
ide_tagged_entry_tag_get_property (GObject      *object,
                                  guint         property_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  IdeTaggedEntryTag *self = IDE_TAGGED_ENTRY_TAG (object);

  switch (property_id)
    {
      case PROP_TAG_LABEL:
        g_value_set_string (value, ide_tagged_entry_tag_get_label (self));
        break;
      case PROP_TAG_HAS_CLOSE_BUTTON:
        g_value_set_boolean (value, ide_tagged_entry_tag_get_has_close_button (self));
        break;
      case PROP_TAG_STYLE:
        g_value_set_string (value, ide_tagged_entry_tag_get_style (self));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ide_tagged_entry_tag_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTaggedEntryTag *self = IDE_TAGGED_ENTRY_TAG (object);

  switch (property_id)
    {
      case PROP_TAG_LABEL:
        ide_tagged_entry_tag_set_label (self, g_value_get_string (value));
        break;
      case PROP_TAG_HAS_CLOSE_BUTTON:
        ide_tagged_entry_tag_set_has_close_button (self, g_value_get_boolean (value));
        break;
      case PROP_TAG_STYLE:
        ide_tagged_entry_tag_set_style (self, g_value_get_string (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
ide_tagged_entry_tag_class_init (IdeTaggedEntryTagClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = ide_tagged_entry_tag_finalize;
  oclass->set_property = ide_tagged_entry_tag_set_property;
  oclass->get_property = ide_tagged_entry_tag_get_property;

  tag_properties[PROP_TAG_LABEL] =
    g_param_spec_string ("label", "Label",
                         "Text to show on the tag.", NULL,
                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  tag_properties[PROP_TAG_HAS_CLOSE_BUTTON] =
    g_param_spec_boolean ("has-close-button", "Tag has a close button",
                          "Whether the tag has a close button.", TRUE,
                          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  tag_properties[PROP_TAG_STYLE] =
    g_param_spec_string ("style", "Style",
                         "Style of the tag.", "entry-tag",
                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, NUM_TAG_PROPERTIES, tag_properties);
}

IdeTaggedEntry *
ide_tagged_entry_new (void)
{
  return g_object_new (IDE_TYPE_TAGGED_ENTRY, NULL);
}

gboolean
ide_tagged_entry_insert_tag (IdeTaggedEntry    *self,
                            IdeTaggedEntryTag *tag,
                            gint              position)
{
  if (g_list_find (self->priv->tags, tag) != NULL)
    return FALSE;

  tag->priv->entry = self;

  self->priv->tags = g_list_insert (self->priv->tags, g_object_ref (tag), position);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    ide_tagged_entry_tag_realize (tag, self);

  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    gdk_window_show_unraised (tag->priv->window);

  gtk_widget_queue_resize (GTK_WIDGET (self));

  return TRUE;
}

gboolean
ide_tagged_entry_add_tag (IdeTaggedEntry    *self,
                         IdeTaggedEntryTag *tag)
{
  return ide_tagged_entry_insert_tag (self, tag, -1);
}

gboolean
ide_tagged_entry_remove_tag (IdeTaggedEntry    *self,
                            IdeTaggedEntryTag *tag)
{
  if (!g_list_find (self->priv->tags, tag))
    return FALSE;

  ide_tagged_entry_tag_unrealize (tag);

  self->priv->tags = g_list_remove (self->priv->tags, tag);
  g_object_unref (tag);

  gtk_widget_queue_resize (GTK_WIDGET (self));

  return TRUE;
}

IdeTaggedEntryTag *
ide_tagged_entry_tag_new (const gchar *label)
{
  return g_object_new (IDE_TYPE_TAGGED_ENTRY_TAG, "label", label, NULL);
}

void
ide_tagged_entry_tag_set_label (IdeTaggedEntryTag *tag,
                               const gchar *label)
{
  IdeTaggedEntryTagPrivate *priv;

  g_return_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag));

  priv = tag->priv;

  if (g_strcmp0 (priv->label, label) != 0)
    {
      GtkWidget *entry;

      g_free (priv->label);
      priv->label = g_strdup (label);
      g_clear_object (&priv->layout);

      entry = GTK_WIDGET (tag->priv->entry);
      if (entry)
        gtk_widget_queue_resize (entry);
    }
}

const gchar *
ide_tagged_entry_tag_get_label (IdeTaggedEntryTag *tag)
{
  g_return_val_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag), NULL);

  return tag->priv->label;
}

void
ide_tagged_entry_tag_set_has_close_button (IdeTaggedEntryTag *tag,
                                          gboolean has_close_button)
{
  IdeTaggedEntryTagPrivate *priv;

  g_return_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag));

  priv = tag->priv;

  has_close_button = has_close_button != FALSE;
  if (priv->has_close_button != has_close_button)
    {
      GtkWidget *entry;

      priv->has_close_button = has_close_button;
      g_clear_object (&priv->layout);

      entry = GTK_WIDGET (priv->entry);
      if (entry)
        gtk_widget_queue_resize (entry);
    }
}

gboolean
ide_tagged_entry_tag_get_has_close_button (IdeTaggedEntryTag *tag)
{
  g_return_val_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag), FALSE);

  return tag->priv->has_close_button;
}

void
ide_tagged_entry_tag_set_style (IdeTaggedEntryTag *tag,
                               const gchar *style)
{
  IdeTaggedEntryTagPrivate *priv;

  g_return_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag));

  priv = tag->priv;

  if (g_strcmp0 (priv->style, style) != 0)
    {
      GtkWidget *entry;

      g_free (priv->style);
      priv->style = g_strdup (style);
      g_clear_object (&priv->layout);

      entry = GTK_WIDGET (tag->priv->entry);
      if (entry)
        gtk_widget_queue_resize (entry);
    }
}

const gchar *
ide_tagged_entry_tag_get_style (IdeTaggedEntryTag *tag)
{
  g_return_val_if_fail (IDE_IS_TAGGED_ENTRY_TAG (tag), NULL);

  return tag->priv->style;
}

void
ide_tagged_entry_set_tag_button_visible (IdeTaggedEntry *self,
                                        gboolean       visible)
{
  g_return_if_fail (IDE_IS_TAGGED_ENTRY (self));

  if (self->priv->button_visible == visible)
    return;

  self->priv->button_visible = visible;
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TAG_BUTTON_VISIBLE]);
}

gboolean
ide_tagged_entry_get_tag_button_visible (IdeTaggedEntry *self)
{
  g_return_val_if_fail (IDE_IS_TAGGED_ENTRY (self), FALSE);

  return self->priv->button_visible;
}
