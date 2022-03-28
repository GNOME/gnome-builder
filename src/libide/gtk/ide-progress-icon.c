/* ide-progress-icon.c
 *
 * Copyright (C) 2015 Igalia S.L.
 * Copyright (C) 2016-2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-progress-icon"

#include "config.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "ide-progress-icon.h"

struct _IdeProgressIcon
{
  GtkDrawingArea parent_instance;
  gdouble        progress;
};

G_DEFINE_TYPE (IdeProgressIcon, ide_progress_icon, GTK_TYPE_DRAWING_AREA)

enum {
  PROP_0,
  PROP_PROGRESS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_progress_icon_draw (GtkDrawingArea *area,
                        cairo_t        *cr,
                        int             width,
                        int             height,
                        gpointer        user_data)
{
  IdeProgressIcon *self = (IdeProgressIcon *)area;
  GtkStyleContext *style_context;
  GdkRGBA rgba;
  gdouble alpha;

  g_assert (IDE_IS_PROGRESS_ICON (self));
  g_assert (cr != NULL);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (area));
  gtk_style_context_get_color (style_context, &rgba);

  alpha = rgba.alpha;
  rgba.alpha = 0.15;
  gdk_cairo_set_source_rgba (cr, &rgba);

  cairo_arc (cr,
             width / 2,
             height / 2,
             width / 2,
             0.0,
             2 * M_PI);
  cairo_fill (cr);

  if (self->progress > 0.0)
    {
      rgba.alpha = alpha;
      gdk_cairo_set_source_rgba (cr, &rgba);

      cairo_arc (cr,
                 width / 2,
                 height / 2,
                 width / 2,
                 (-.5 * M_PI),
                 (2 * self->progress * M_PI) - (.5 * M_PI));

      if (self->progress != 1.0)
        {
          cairo_line_to (cr, width / 2, height / 2);
          cairo_line_to (cr, width / 2, 0);
        }

      cairo_fill (cr);
    }
}

static void
ide_progress_icon_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeProgressIcon *self = IDE_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, ide_progress_icon_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_progress_icon_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeProgressIcon *self = IDE_PROGRESS_ICON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      ide_progress_icon_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_progress_icon_class_init (IdeProgressIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_progress_icon_get_property;
  object_class->set_property = ide_progress_icon_set_property;

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
ide_progress_icon_init (IdeProgressIcon *icon)
{
  g_object_set (icon, "width-request", 16, "height-request", 16, NULL);
  gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
  gtk_widget_set_halign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (icon),
                                  ide_progress_icon_draw,
                                  NULL, NULL);
}

GtkWidget *
ide_progress_icon_new (void)
{
  return g_object_new (IDE_TYPE_PROGRESS_ICON, NULL);
}

gdouble
ide_progress_icon_get_progress (IdeProgressIcon *self)
{
  g_return_val_if_fail (IDE_IS_PROGRESS_ICON (self), 0.0);

  return self->progress;
}

void
ide_progress_icon_set_progress (IdeProgressIcon *self,
                                gdouble          progress)
{
  g_return_if_fail (IDE_IS_PROGRESS_ICON (self));

  progress = CLAMP (progress, 0.0, 1.0);

  if (self->progress != progress)
    {
      self->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}
