/* egg-box.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "egg-box.h"

typedef struct
{
  gint max_width_request;
} EggBoxPrivate;

enum {
  PROP_0,
  PROP_MAX_WIDTH_REQUEST,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (EggBox, egg_box, GTK_TYPE_BOX)

static GParamSpec *properties [LAST_PROP];

static void
egg_box_get_preferred_width (GtkWidget *widget,
                             gint      *min_width,
                             gint      *nat_width)
{
  EggBox *self = (EggBox *)widget;
  EggBoxPrivate *priv = egg_box_get_instance_private (self);

  g_assert (EGG_IS_BOX (self));

  GTK_WIDGET_CLASS (egg_box_parent_class)->get_preferred_width (widget, min_width, nat_width);

  if (priv->max_width_request > 0)
    {
      if (*min_width > priv->max_width_request)
        *min_width = priv->max_width_request;

      if (*nat_width > priv->max_width_request)
        *nat_width = priv->max_width_request;
    }
}

static void
egg_box_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  EggBox *self = EGG_BOX (object);
  EggBoxPrivate *priv = egg_box_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MAX_WIDTH_REQUEST:
      g_value_set_int (value, priv->max_width_request);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_box_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  EggBox *self = EGG_BOX (object);
  EggBoxPrivate *priv = egg_box_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MAX_WIDTH_REQUEST:
      priv->max_width_request = g_value_get_int (value);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_box_class_init (EggBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = egg_box_get_property;
  object_class->set_property = egg_box_set_property;

  widget_class->get_preferred_width = egg_box_get_preferred_width;

  properties [PROP_MAX_WIDTH_REQUEST] =
    g_param_spec_int ("max-width-request",
                      "Max Width Request",
                      "Max Width Request",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
egg_box_init (EggBox *self)
{
  EggBoxPrivate *priv = egg_box_get_instance_private (self);

  priv->max_width_request = -1;
}
