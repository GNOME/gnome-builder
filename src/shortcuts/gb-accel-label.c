/* gb-accel-label.c
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

#include "gb-accel-label.h"
#include "gb-widget.h"

struct _GbAccelLabel
{
  GtkBox  parent_instance;
  gchar  *accelerator;
};

G_DEFINE_TYPE (GbAccelLabel, gb_accel_label, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_accel_label_rebuild (GbAccelLabel *self)
{
  g_auto(GStrv) keys = NULL;
  g_autofree gchar *label = NULL;
  GdkModifierType modifier = 0;
  guint key = 0;
  guint i;

  g_assert (GB_IS_ACCEL_LABEL (self));

  gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);

  if (self->accelerator == NULL)
    return;

  gtk_accelerator_parse (self->accelerator, &key, &modifier);
  if ((key == 0) && (modifier == 0))
    return;

  label = gtk_accelerator_get_label (key, modifier);
  if (label == NULL)
    return;

  keys = g_strsplit (label, "+", 0);

  for (i = 0; keys [i]; i++)
    {
      GtkFrame *frame;
      GtkLabel *disp;

      if (i > 0)
        {
          GtkLabel *plus;

          plus = g_object_new (GTK_TYPE_LABEL,
                               "label", "+",
                               "visible", TRUE,
                               NULL);
          gb_widget_add_style_class (GTK_WIDGET (plus), "dim-label");
          gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (plus));
        }

      frame = g_object_new (GTK_TYPE_FRAME,
                            "visible", TRUE,
                            NULL);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (frame));

      /*
       * FIXME: Check if the item is a modifier.
       *
       * If we have a size group, size everything the same except for the
       * last item. This has the side effect of basically matching all
       * modifiers together. Not always the case, but simple and easy
       * hack.
       */
      if (keys [i + 1] != NULL)
        gtk_widget_set_size_request (GTK_WIDGET (frame), 50, -1);

      disp = g_object_new (GTK_TYPE_LABEL,
                           "label", keys [i],
                           "visible", TRUE,
                           NULL);
      gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (disp));
    }
}

static void
gb_accel_label_finalize (GObject *object)
{
  GbAccelLabel *self = (GbAccelLabel *)object;

  g_clear_pointer (&self->accelerator, g_free);

  G_OBJECT_CLASS (gb_accel_label_parent_class)->finalize (object);
}

static void
gb_accel_label_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbAccelLabel *self = GB_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      g_value_set_string (value, gb_accel_label_get_accelerator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_accel_label_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbAccelLabel *self = GB_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gb_accel_label_set_accelerator (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_accel_label_class_init (GbAccelLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_accel_label_finalize;
  object_class->get_property = gb_accel_label_get_property;
  object_class->set_property = gb_accel_label_set_property;

  gParamSpecs [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         "Accelerator",
                         "Accelerator",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_accel_label_init (GbAccelLabel *self)
{
  gtk_box_set_spacing (GTK_BOX (self), 6);
}

GtkWidget *
gb_accel_label_new (const gchar *accelerator)
{
  return g_object_new (GB_TYPE_ACCEL_LABEL,
                       "accelerator", accelerator,
                       NULL);
}

const gchar *
gb_accel_label_get_accelerator (GbAccelLabel *self)
{
  g_return_val_if_fail (GB_IS_ACCEL_LABEL (self), NULL);

  return self->accelerator;
}

void
gb_accel_label_set_accelerator (GbAccelLabel *self,
                                const gchar  *accelerator)
{
  g_return_if_fail (GB_IS_ACCEL_LABEL (self));

  if (g_strcmp0 (accelerator, self->accelerator) != 0)
    {
      g_free (self->accelerator);
      self->accelerator = g_strdup (accelerator);
      gb_accel_label_rebuild (self);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ACCELERATOR]);
    }
}
