/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
 *  Copyright © 2016 Christian Hergert <chergert@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define G_LOG_DOMAIN "ide-transfers-progress-icon"

#include "transfers/ide-transfers-progress-icon.h"

struct _IdeTransfersProgressIcon
{
  GtkDrawingArea parent_instance;
  gdouble        progress;
};

G_DEFINE_TYPE (IdeTransfersProgressIcon, ide_transfers_progress_icon, GTK_TYPE_DRAWING_AREA)

enum {
  PROP_0,
  PROP_PROGRESS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
ide_transfers_progress_icon_draw (GtkWidget *widget,
                                  cairo_t   *cr)
{
  IdeTransfersProgressIcon *self = (IdeTransfersProgressIcon *)widget;
  GtkStyleContext *style_context;
  GdkRGBA color;
  gdouble progress;
  gint width;
  gint height;

  g_assert (IDE_IS_TRANSFERS_PROGRESS_ICON (self));
  g_assert (cr != NULL);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  progress = ide_transfers_progress_icon_get_progress (self);

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (style_context, gtk_widget_get_state_flags (widget), &color);
  color.alpha *= progress == 1 ? 1 : 0.2;

  gdk_cairo_set_source_rgba (cr, &color);
  cairo_move_to (cr, width / 4., 0);
  cairo_line_to (cr, width - (width / 4.), 0);
  cairo_line_to (cr, width - (width / 4.), height / 2.);
  cairo_line_to (cr, width, height / 2.);
  cairo_line_to (cr, width / 2., height);
  cairo_line_to (cr, 0, height / 2.);
  cairo_line_to (cr, width / 4., height / 2.);
  cairo_line_to (cr, width / 4., 0);
  cairo_fill_preserve (cr);

  if (progress > 0 && progress < 1)
    {
      cairo_clip (cr);
      color.alpha = 1;
      gdk_cairo_set_source_rgba (cr, &color);
      cairo_rectangle (cr, 0, 0, width, height * progress);
      cairo_fill (cr);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_transfers_progress_icon_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeTransfersProgressIcon *self = IDE_TRANSFERS_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, ide_transfers_progress_icon_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfers_progress_icon_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeTransfersProgressIcon *self = IDE_TRANSFERS_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      ide_transfers_progress_icon_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfers_progress_icon_class_init (IdeTransfersProgressIconClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_transfers_progress_icon_get_property;
  object_class->set_property = ide_transfers_progress_icon_set_property;

  widget_class->draw = ide_transfers_progress_icon_draw;

  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "Progress",
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_transfers_progress_icon_init (IdeTransfersProgressIcon *icon)
{
  g_object_set (icon, "width-request", 16, "height-request", 16, NULL);
  gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
}

GtkWidget *
ide_transfers_progress_icon_new (void)
{
  return g_object_new (IDE_TYPE_TRANSFERS_PROGRESS_ICON, NULL);
}

gdouble
ide_transfers_progress_icon_get_progress (IdeTransfersProgressIcon *self)
{
  g_return_val_if_fail (IDE_IS_TRANSFERS_PROGRESS_ICON (self), 0.0);

  return self->progress;
}

void
ide_transfers_progress_icon_set_progress (IdeTransfersProgressIcon *self,
                                          gdouble                   progress)
{
  g_return_if_fail (IDE_IS_TRANSFERS_PROGRESS_ICON (self));

  progress = CLAMP (progress, 0.0, 1.0);

  if (self->progress != progress)
    {
      self->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}
