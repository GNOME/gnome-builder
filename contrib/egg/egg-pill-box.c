/* egg-pill-box.c
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

#define G_LOG_DOMAIN "egg-pill-box"

#include <glib/gi18n.h>

#include "egg-pill-box.h"

struct _EggPillBox
{
  GtkEventBox  parent_instance;

  GtkLabel    *label;
};

G_DEFINE_TYPE (EggPillBox, egg_pill_box, GTK_TYPE_EVENT_BOX)

enum {
  PROP_0,
  PROP_LABEL,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

const gchar *
egg_pill_box_get_label (EggPillBox *self)
{
  g_return_val_if_fail (EGG_IS_PILL_BOX (self), NULL);

  return gtk_label_get_label (self->label);
}

void
egg_pill_box_set_label (EggPillBox  *self,
                        const gchar *label)
{
  g_return_if_fail (EGG_IS_PILL_BOX (self));

  gtk_label_set_label (self->label, label);
}

GtkWidget *
egg_pill_box_new (const gchar *label)
{
  return g_object_new (EGG_TYPE_PILL_BOX,
                       "label", label,
                       NULL);
}

static void
egg_pill_box_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  EggPillBox *self = EGG_PILL_BOX (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, egg_pill_box_get_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_pill_box_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  EggPillBox *self = EGG_PILL_BOX (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      egg_pill_box_set_label (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_pill_box_class_init (EggPillBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = egg_pill_box_get_property;
  object_class->set_property = egg_pill_box_set_property;

  properties [PROP_LABEL] =
    g_param_spec_string ("label",
                         "Label",
                         "The label for the pill box.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LABEL, properties [PROP_LABEL]);

  gtk_widget_class_set_css_name (widget_class, "pillbox");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libegg-private/egg-pill-box.ui");
  gtk_widget_class_bind_template_child (widget_class, EggPillBox, label);
}

static void
egg_pill_box_init (EggPillBox *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
