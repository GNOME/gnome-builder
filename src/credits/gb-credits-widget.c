/* gb-credits-widget.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-animation.h"
#include "gb-credits-widget.h"
#include "gb-widget.h"

struct _GbCreditsWidgetPrivate
{
  GbAnimation *animation;
  GtkGrid     *grid;
  GtkEventBox *event_box;
  gdouble      progress;
  guint        duration;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCreditsWidget, gb_credits_widget,
                            GTK_TYPE_OVERLAY)

enum {
  PROP_0,
  PROP_PROGRESS,
  PROP_DURATION,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_credits_widget_new (void)
{
  return g_object_new (GB_TYPE_CREDITS_WIDGET, NULL);
}

static void
stop_animation (GbCreditsWidget *widget)
{
  GbCreditsWidgetPrivate *priv;
  GbAnimation *anim;

  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));

  priv = widget->priv;

  if ((anim = priv->animation))
    {
      g_object_remove_weak_pointer (G_OBJECT (anim),
                                    (gpointer *)&priv->animation);
      priv->animation = NULL;
      gb_animation_stop (anim);
    }
}

void
gb_credits_widget_stop (GbCreditsWidget *widget)
{
  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));

  stop_animation (widget);

  if (gtk_widget_get_visible (GTK_WIDGET (widget)))
    gb_widget_fade_hide (GTK_WIDGET (widget));
}

static void
animation_finished (gpointer data)
{
  GbCreditsWidget *widget =  data;

  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));

  gb_credits_widget_stop (widget);
}

void
gb_credits_widget_start (GbCreditsWidget *widget)
{
  GbCreditsWidgetPrivate *priv;
  GdkFrameClock *frame_clock;

  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));

  priv = widget->priv;

  stop_animation (widget);

  gb_credits_widget_set_progress (widget, 0.0);

  gb_widget_fade_show (GTK_WIDGET (widget));

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (priv->grid));

  priv->animation = gb_object_animate_full (widget,
                                            GB_ANIMATION_LINEAR,
                                            priv->duration,
                                            frame_clock,
                                            animation_finished,
                                            widget,
                                            "progress", 1.0,
                                            NULL);
  g_object_add_weak_pointer (G_OBJECT (priv->animation),
                             (gpointer *)&priv->animation);
}

guint
gb_credits_widget_get_duration (GbCreditsWidget *widget)
{
  g_return_val_if_fail (GB_IS_CREDITS_WIDGET (widget), 0);

  return widget->priv->duration;
}

void
gb_credits_widget_set_duration (GbCreditsWidget *widget,
                                guint            duration)
{
  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));
  g_return_if_fail (duration > 1000);

  if (widget->priv->duration != duration)
    {
      widget->priv->duration = duration;
      g_object_notify_by_pspec (G_OBJECT (widget), gParamSpecs [PROP_DURATION]);
    }
}

gdouble
gb_credits_widget_get_progress (GbCreditsWidget *widget)
{
  g_return_val_if_fail (GB_IS_CREDITS_WIDGET (widget), 0.0);

  return widget->priv->progress;
}

void
gb_credits_widget_set_progress (GbCreditsWidget *widget,
                                gdouble          progress)
{
  g_return_if_fail (GB_IS_CREDITS_WIDGET (widget));

  progress = CLAMP (progress, 0.0, 1.0);

  if (progress != widget->priv->progress)
    {
      widget->priv->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (widget), gParamSpecs [PROP_PROGRESS]);
      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }
}

static gboolean
gb_credits_widget_get_child_position (GtkOverlay    *overlay,
                                      GtkWidget     *widget,
                                      GtkAllocation *allocation)
{
  GtkRequisition natural_size;
  GtkAllocation my_alloc;
  gdouble progress;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (allocation, FALSE);

  gtk_widget_get_allocation (GTK_WIDGET (overlay), &my_alloc);

  progress = gb_credits_widget_get_progress (GB_CREDITS_WIDGET (overlay));

  gtk_widget_get_preferred_size (widget, NULL, &natural_size);

  allocation->width = MAX (my_alloc.width, natural_size.width);
  allocation->x = (my_alloc.width - allocation->width) / 2;
  allocation->height = natural_size.height;
  allocation->y = -(natural_size.height * progress);

  return TRUE;
}

static void
gb_credits_widget_dispose (GObject *object)
{
  GbCreditsWidget *widget = (GbCreditsWidget *)object;

  stop_animation (widget);

  G_OBJECT_CLASS (gb_credits_widget_parent_class)->dispose (object);
}

static void
gb_credits_widget_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbCreditsWidget *self = GB_CREDITS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, gb_credits_widget_get_duration (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, gb_credits_widget_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_credits_widget_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbCreditsWidget *self = GB_CREDITS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      gb_credits_widget_set_duration (self, g_value_get_uint (value));
      break;

    case PROP_PROGRESS:
      gb_credits_widget_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_credits_widget_class_init (GbCreditsWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkOverlayClass *overlay_class = GTK_OVERLAY_CLASS (klass);

  object_class->dispose = gb_credits_widget_dispose;
  object_class->get_property = gb_credits_widget_get_property;
  object_class->set_property = gb_credits_widget_set_property;

  overlay_class->get_child_position = gb_credits_widget_get_child_position;

  gParamSpecs [PROP_DURATION] =
    g_param_spec_uint ("duration",
                       _("Duration"),
                       _("The duration of the animation in millseconds."),
                       0,
                       G_MAXUINT,
                       20000,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DURATION,
                                   gParamSpecs [PROP_DURATION]);

  gParamSpecs [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         _("Progress"),
                         _("Progress"),
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PROGRESS,
                                   gParamSpecs [PROP_PROGRESS]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-credits-widget.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbCreditsWidget, grid);
  gtk_widget_class_bind_template_child_private (widget_class, GbCreditsWidget, event_box);
}

static void
gb_credits_widget_init (GbCreditsWidget *self)
{
  self->priv = gb_credits_widget_get_instance_private (self);

  self->priv->duration = 1000 * 20;

  gtk_widget_init_template (GTK_WIDGET (self));
}
