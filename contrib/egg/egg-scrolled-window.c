/* egg-scrolled-window.c
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

#include "egg-scrolled-window.h"

typedef struct
{
  gint max_content_height;
  gint max_content_width;
} EggScrolledWindowPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EggScrolledWindow, egg_scrolled_window, GTK_TYPE_SCROLLED_WINDOW)

enum {
  PROP_0,
  PROP_MAX_CONTENT_HEIGHT,
  PROP_MAX_CONTENT_WIDTH,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkWidget *
egg_scrolled_window_new (void)
{
  return g_object_new (EGG_TYPE_SCROLLED_WINDOW, NULL);
}

gint
egg_scrolled_window_get_max_content_height (EggScrolledWindow *self)
{
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_SCROLLED_WINDOW (self), -1);

  return priv->max_content_height;
}

/**
 * egg_scrolled_window_set_max_content_height:
 * @max_content_height: the max allowed height request or -1 to ignore.
 *
 * This function will set the "max-content-height" property. This property is
 * used to determine the maximum height that the scrolled window will request.
 *
 * This is useful if you want to have a scrolled window grow with the child
 * allocation, but only up to a certain height.
 */
void
egg_scrolled_window_set_max_content_height (EggScrolledWindow *self,
                                            gint               max_content_height)
{
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_if_fail (EGG_IS_SCROLLED_WINDOW (self));

  if (max_content_height != priv->max_content_height)
    {
      priv->max_content_height = max_content_height;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_CONTENT_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

gint
egg_scrolled_window_get_max_content_width (EggScrolledWindow *self)
{
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_SCROLLED_WINDOW (self), -1);

  return priv->max_content_width;
}

/**
 * egg_scrolled_window_set_max_content_width:
 * @max_content_width: the max allowed width request or -1 to ignore.
 *
 * This function will set the "max-content-width" property. This property is
 * used to determine the maximum width that the scrolled window will request.
 *
 * This is useful if you want to have a scrolled window grow with the child
 * allocation, but only up to a certain width.
 */
void
egg_scrolled_window_set_max_content_width (EggScrolledWindow *self,
                                           gint               max_content_width)
{
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_if_fail (EGG_IS_SCROLLED_WINDOW (self));

  if (max_content_width != priv->max_content_width)
    {
      priv->max_content_width = max_content_width;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_CONTENT_HEIGHT]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
egg_scrolled_window_get_preferred_height (GtkWidget *widget,
                                          gint      *minimum_height,
                                          gint      *natural_height)
{
  EggScrolledWindow *self = (EggScrolledWindow *)widget;
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_if_fail (EGG_IS_SCROLLED_WINDOW (self));

  GTK_WIDGET_CLASS (egg_scrolled_window_parent_class)->get_preferred_height (widget, minimum_height, natural_height);

  if (natural_height)
    {
      if (priv->max_content_height > -1)
        {
          GtkWidget *child;
          GtkStyleContext *style;
          GtkBorder border;
          gint child_min_height;
          gint child_nat_height;
          gint additional;

          if (!(child = gtk_bin_get_child (GTK_BIN (widget))))
            return;

          style = gtk_widget_get_style_context (widget);
          gtk_style_context_get_border (style, gtk_widget_get_state_flags (widget), &border);
          additional = border.top + border.bottom;

          gtk_widget_get_preferred_height (child, &child_min_height, &child_nat_height);

          if ((child_nat_height > *natural_height) && (priv->max_content_height > *natural_height))
            *natural_height = MIN (priv->max_content_height, child_nat_height) + additional;
        }
    }
}

static void
egg_scrolled_window_get_preferred_width (GtkWidget *widget,
                                         gint      *minimum_width,
                                         gint      *natural_width)
{
  EggScrolledWindow *self = (EggScrolledWindow *)widget;
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  g_return_if_fail (EGG_IS_SCROLLED_WINDOW (self));

  GTK_WIDGET_CLASS (egg_scrolled_window_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

  if (natural_width)
    {
      if (priv->max_content_width > -1)
        {
          GtkWidget *child;
          GtkStyleContext *style;
          GtkBorder border;
          gint child_min_width;
          gint child_nat_width;
          gint additional;

          if (!(child = gtk_bin_get_child (GTK_BIN (widget))))
            return;

          style = gtk_widget_get_style_context (widget);
          gtk_style_context_get_border (style, gtk_widget_get_state_flags (widget), &border);
          additional = border.left = border.right + 1;

          gtk_widget_get_preferred_width (child, &child_min_width, &child_nat_width);

          if ((child_nat_width > *natural_width) && (priv->max_content_width > *natural_width))
            *natural_width = MIN (priv->max_content_width, child_nat_width) + additional;
        }
    }
}

static void
egg_scrolled_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EggScrolledWindow *self = EGG_SCROLLED_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MAX_CONTENT_HEIGHT:
      g_value_set_int (value, egg_scrolled_window_get_max_content_height (self));
      break;

    case PROP_MAX_CONTENT_WIDTH:
      g_value_set_int (value, egg_scrolled_window_get_max_content_width (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_scrolled_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EggScrolledWindow *self = EGG_SCROLLED_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MAX_CONTENT_HEIGHT:
      egg_scrolled_window_set_max_content_height (self, g_value_get_int (value));
      break;

    case PROP_MAX_CONTENT_WIDTH:
      egg_scrolled_window_set_max_content_width (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_scrolled_window_class_init (EggScrolledWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = egg_scrolled_window_get_property;
  object_class->set_property = egg_scrolled_window_set_property;

  widget_class->get_preferred_width = egg_scrolled_window_get_preferred_width;
  widget_class->get_preferred_height = egg_scrolled_window_get_preferred_height;

  properties [PROP_MAX_CONTENT_HEIGHT] =
    g_param_spec_int ("max-content-height",
                      "Max Content Height",
                      "The maximum height request that can be made.",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MAX_CONTENT_WIDTH] =
    g_param_spec_int ("max-content-width",
                      "Max Content Width",
                      "The maximum width request that can be made.",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
egg_scrolled_window_init (EggScrolledWindow *self)
{
  EggScrolledWindowPrivate *priv = egg_scrolled_window_get_instance_private (self);

  priv->max_content_height = -1;
  priv->max_content_width = -1;
}
