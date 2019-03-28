/* ide-completion-overlay.c
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

#define G_LOG_DOMAIN "ide-completion-overlay"

#include "config.h"

#include "ide-completion-display.h"
#include "ide-completion-overlay.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"
#include "ide-completion-view.h"

struct _IdeCompletionOverlay
{
  DzlBin             parent_instance;
  IdeCompletionView *view;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static void completion_display_iface_init (IdeCompletionDisplayInterface *);

G_DEFINE_TYPE_WITH_CODE (IdeCompletionOverlay, ide_completion_overlay, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_DISPLAY,
                                                completion_display_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_completion_overlay_show (GtkWidget *widget)
{
  gtk_widget_set_opacity (widget, 1.0);

  GTK_WIDGET_CLASS (ide_completion_overlay_parent_class)->show (widget);
}

static void
ide_completion_overlay_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCompletionOverlay *self = IDE_COMPLETION_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_completion_view_get_context (self->view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_overlay_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeCompletionOverlay *self = IDE_COMPLETION_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_completion_view_set_context (self->view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_overlay_class_init (IdeCompletionOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_completion_overlay_get_property;
  object_class->set_property = ide_completion_overlay_set_property;

  widget_class->show = ide_completion_overlay_show;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context to be displayed",
                         IDE_TYPE_COMPLETION_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "completionoverlay");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-sourceview/ui/ide-completion-overlay.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionOverlay, view);

  g_type_ensure (IDE_TYPE_COMPLETION_VIEW);
}

static void
ide_completion_overlay_init (IdeCompletionOverlay *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);

  g_signal_connect_swapped (self->view,
                            "reposition",
                            G_CALLBACK (gtk_widget_queue_resize),
                            self);
}

IdeCompletionOverlay *
_ide_completion_overlay_new (void)
{
  return g_object_new (IDE_TYPE_COMPLETION_OVERLAY, NULL);
}

static gboolean
ide_completion_overlay_get_child_position_cb (IdeCompletionOverlay *self,
                                              GtkWidget            *widget,
                                              GdkRectangle         *out_rect,
                                              GtkOverlay           *overlay)
{
  IdeCompletionContext *context;
  GtkStyleContext *style_context;
  GdkRectangle begin_rect, end_rect, rect;
  GtkTextIter begin, end;
  GtkBorder border;
  GtkAllocation alloc;
  GtkStateFlags flags;
  GtkRequisition min, nat;
  GtkTextView *view;
  gint x_offset = 0;

  g_return_val_if_fail (IDE_IS_COMPLETION_OVERLAY (self), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (out_rect != NULL, FALSE);

  if (widget != GTK_WIDGET (self))
    return FALSE;

  if (!(context = ide_completion_view_get_context (self->view)))
    return FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (overlay), &alloc);

  view = ide_completion_context_get_view (context);

  gtk_widget_get_preferred_size (widget, &min, &nat);

  ide_completion_context_get_bounds (context, &begin, &end);

  gtk_text_view_get_iter_location (view, &begin, &begin_rect);
  gtk_text_view_get_iter_location (view, &end, &end_rect);
  gtk_text_view_buffer_to_window_coords (view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         begin_rect.x, begin_rect.y,
                                         &begin_rect.x, &begin_rect.y);
  gtk_text_view_buffer_to_window_coords (view,
                                         GTK_TEXT_WINDOW_WIDGET,
                                         end_rect.x, end_rect.y,
                                         &end_rect.x, &end_rect.y);
  gdk_rectangle_union (&begin_rect, &end_rect, &rect);
  gtk_widget_translate_coordinates (GTK_WIDGET (view), GTK_WIDGET (overlay),
                                    rect.x, rect.y,
                                    &rect.x, &rect.y);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->view));
  flags = gtk_style_context_get_state (style_context);
  gtk_style_context_get_margin (style_context, flags, &border);

  x_offset = _ide_completion_view_get_x_offset (self->view);

/* TODO: Figure out where 11 is coming from */
#define EXTRA_SHIFT 11
  x_offset -= EXTRA_SHIFT;

  out_rect->x = rect.x - x_offset - border.left;
  out_rect->y = rect.y + rect.height;
  out_rect->height = nat.height;
  out_rect->width = nat.width;

  /*
   * If we can keep the position in place by using the minimum size (or
   * larger up to the overlay bounds), then prefer to do that before shifting.
   */
  if (out_rect->x + out_rect->width > alloc.width)
    {
      if (out_rect->x + min.width <= alloc.width)
        out_rect->width = alloc.width - out_rect->x;
      else
        out_rect->width = alloc.width - min.width;
    }

  if (out_rect->x < 0)
    {
      out_rect->x = 0;
      if (out_rect->width > alloc.width)
        out_rect->width = alloc.width;
    }

  if (out_rect->y + out_rect->height > alloc.height)
    out_rect->y = rect.y - out_rect->height;

#if 0
  g_print ("Position: %d,%d  %dx%d\n",
           out_rect->x,
           out_rect->y,
           out_rect->width,
           out_rect->height);
#endif

  return TRUE;
}

static void
ide_completion_overlay_attach (IdeCompletionDisplay *display,
                               GtkSourceView        *view)
{
  IdeCompletionOverlay *self = (IdeCompletionOverlay *)display;
  GtkOverlay *overlay = NULL;
  GtkWidget *widget = (GtkWidget *)view;

  g_assert (IDE_IS_COMPLETION_OVERLAY (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  while ((widget = gtk_widget_get_ancestor (widget, GTK_TYPE_OVERLAY)))
    {
      overlay = GTK_OVERLAY (widget);
      widget = gtk_widget_get_parent (widget);
    }

  if (overlay == NULL)
    {
      g_critical ("IdeCompletion requires a GtkOverlay to attach the completion "
                  "window due to resize restrictions in windowing systems");
      return;
    }

  gtk_overlay_add_overlay (overlay, GTK_WIDGET (self));

  g_signal_connect_object (overlay,
                           "get-child-position",
                           G_CALLBACK (ide_completion_overlay_get_child_position_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_completion_overlay_set_n_rows (IdeCompletionDisplay *display,
                                   guint                 n_rows)
{
  g_assert (IDE_IS_COMPLETION_OVERLAY (display));
  g_assert (n_rows > 0);
  g_assert (n_rows <= 32);

  _ide_completion_view_set_n_rows (IDE_COMPLETION_OVERLAY (display)->view, n_rows);
}

static gboolean
ide_completion_overlay_key_press_event (IdeCompletionDisplay *display,
                                        const GdkEventKey    *event)
{
  IdeCompletionOverlay *self = (IdeCompletionOverlay *)display;

  g_assert (IDE_IS_COMPLETION_OVERLAY (self));
  g_assert (event != NULL);

  return _ide_completion_view_handle_key_press (self->view, event);
}

static void
ide_completion_overlay_set_context (IdeCompletionDisplay *display,
                                    IdeCompletionContext *context)
{
  IdeCompletionOverlay *self = (IdeCompletionOverlay *)display;

  g_return_if_fail (IDE_IS_COMPLETION_OVERLAY (self));
  g_return_if_fail (!context || IDE_IS_COMPLETION_CONTEXT (context));

  ide_completion_view_set_context (self->view, context);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
}

static void
ide_completion_overlay_move_cursor (IdeCompletionDisplay *display,
                                    GtkMovementStep       step,
                                    gint                  count)
{
  g_assert (IDE_IS_COMPLETION_OVERLAY (display));

  _ide_completion_view_move_cursor (IDE_COMPLETION_OVERLAY (display)->view, step, count);
}

static void
ide_completion_overlay_set_font_desc (IdeCompletionDisplay       *display,
                                      const PangoFontDescription *font_desc)
{
  g_assert (IDE_IS_COMPLETION_OVERLAY (display));

  _ide_completion_view_set_font_desc (IDE_COMPLETION_OVERLAY (display)->view, font_desc);
}

static void
completion_display_iface_init (IdeCompletionDisplayInterface *iface)
{
  iface->set_context = ide_completion_overlay_set_context;
  iface->attach = ide_completion_overlay_attach;
  iface->key_press_event = ide_completion_overlay_key_press_event;
  iface->set_n_rows = ide_completion_overlay_set_n_rows;
  iface->move_cursor = ide_completion_overlay_move_cursor;
  iface->set_font_desc = ide_completion_overlay_set_font_desc;
}
