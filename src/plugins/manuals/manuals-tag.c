/*
 * manuals-tag.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-tag.h"
#include "manuals-utils.h"

struct _ManualsTag
{
  GtkWidget parent_instance;

  PangoLayout *cached_layout;

  char *key;
  char *value;
};

G_DEFINE_FINAL_TYPE (ManualsTag, manuals_tag, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_KEY,
  PROP_VALUE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static PangoLayout *
manuals_tag_create_layout (ManualsTag *self)
{
  g_assert (MANUALS_IS_TAG (self));

  if (self->cached_layout == NULL)
    {
      g_autoptr(GString) gstring = g_string_new (self->key);

      if (!_g_str_empty0 (self->value))
        {
          const char *colon;

          if (gstring->len > 0)
            g_string_append_len (gstring, ": ", 2);

          if ((colon = strchr (self->value, ':')))
            g_string_append_len (gstring, self->value, colon - self->value);
          else
            g_string_append (gstring, self->value);

          while (gstring->str[gstring->len-1] == ' ')
            g_string_truncate (gstring, gstring->len-1);
        }

      self->cached_layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), gstring->str);
    }

  return g_object_ref (self->cached_layout);
}

static void
manuals_tag_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  ManualsTag *self = (ManualsTag *)widget;
  g_autoptr(PangoLayout) layout = NULL;
  int width;
  int height;

  g_assert (MANUALS_IS_TAG (self));
  g_assert (minimum != NULL);
  g_assert (natural != NULL);
  g_assert (minimum_baseline != NULL);
  g_assert (natural_baseline != NULL);

  layout = manuals_tag_create_layout (self);

  pango_layout_get_pixel_size (layout, &width, &height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = *natural = width;
      *minimum_baseline = *natural_baseline = -1;
    }
  else
    {
      *minimum = *natural = height;
      *minimum_baseline = *natural_baseline = -1;
    }
}

static void
manuals_tag_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  ManualsTag *self = (ManualsTag *)widget;
  g_autoptr(PangoLayout) layout = NULL;
  GdkRGBA color;

  g_assert (MANUALS_IS_TAG (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  layout = manuals_tag_create_layout (self);

  gtk_widget_get_color (widget, &color);

  gtk_snapshot_append_layout (snapshot, layout, &color);
}

static void
manuals_tag_dispose (GObject *object)
{
  ManualsTag *self = (ManualsTag *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->value, g_free);
  g_clear_object (&self->cached_layout);

  G_OBJECT_CLASS (manuals_tag_parent_class)->dispose (object);
}

static void
manuals_tag_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ManualsTag *self = MANUALS_TAG (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, manuals_tag_get_key (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, manuals_tag_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_tag_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ManualsTag *self = MANUALS_TAG (object);

  switch (prop_id)
    {
    case PROP_KEY:
      manuals_tag_set_key (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      manuals_tag_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_tag_class_init (ManualsTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = manuals_tag_dispose;
  object_class->get_property = manuals_tag_get_property;
  object_class->set_property = manuals_tag_set_property;

  widget_class->measure = manuals_tag_measure;
  widget_class->snapshot = manuals_tag_snapshot;

  properties[PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "tag");
}

static void
manuals_tag_init (ManualsTag *self)
{
}

const char *
manuals_tag_get_key (ManualsTag *self)
{
  g_return_val_if_fail (MANUALS_IS_TAG (self), NULL);

  return self->key;
}

void
manuals_tag_set_key (ManualsTag *self,
                     const char *key)
{
  g_return_if_fail (MANUALS_IS_TAG (self));

  if (g_set_str (&self->key, key))
    {
      g_clear_object (&self->cached_layout);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEY]);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

const char *
manuals_tag_get_value (ManualsTag *self)
{
  g_return_val_if_fail (MANUALS_IS_TAG (self), NULL);

  return self->value;
}

void
manuals_tag_set_value (ManualsTag *self,
                       const char *value)
{
  g_return_if_fail (MANUALS_IS_TAG (self));

  if (g_set_str (&self->value, value))
    {
      g_clear_object (&self->cached_layout);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
      gtk_widget_set_visible (GTK_WIDGET (self), !_g_str_empty0 (value));
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}
