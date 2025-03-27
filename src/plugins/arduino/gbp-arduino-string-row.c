/*
 * gbp-arduino-library-editor-row.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gbp-arduino-string-row"

#include "config.h"

#include "gbp-arduino-string-row.h"

struct _GbpArduinoStringRow
{
  GtkListBoxRow parent_instance;

  char *library_name;

  GtkBox    *box;
  GtkLabel  *name_label;
  GtkButton *remove_button;
};

enum
{
  PROP_0,
  PROP_LIBRARY_NAME,
  N_PROPS
};

enum
{
  REMOVE,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (GbpArduinoStringRow, gbp_arduino_string_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
remove_button_clicked (GtkButton *button,
                       GbpArduinoStringRow *self)
{
  g_assert (GBP_IS_ARDUINO_STRING_ROW (self));
  g_assert (GTK_IS_BUTTON (button));

  g_signal_emit (self, signals[REMOVE], 0);
}

static void
gbp_arduino_string_row_dispose (GObject *object)
{
  GbpArduinoStringRow *self = (GbpArduinoStringRow *) object;

  g_clear_pointer ((GtkWidget **) &self->box, gtk_widget_unparent);
  g_clear_pointer (&self->library_name, g_free);

  G_OBJECT_CLASS (gbp_arduino_string_row_parent_class)->dispose (object);
}

static void
gbp_arduino_string_row_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpArduinoStringRow *self = GBP_ARDUINO_STRING_ROW (object);

  switch (prop_id)
    {
    case PROP_LIBRARY_NAME:
      g_value_set_string (value, self->library_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_string_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpArduinoStringRow *self = GBP_ARDUINO_STRING_ROW (object);

  switch (prop_id)
    {
    case PROP_LIBRARY_NAME:
      g_set_str (&self->library_name, g_value_get_string (value));
      if (self->name_label != NULL)
        gtk_label_set_text (self->name_label, self->library_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_string_row_class_init (GbpArduinoStringRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_arduino_string_row_dispose;
  object_class->get_property = gbp_arduino_string_row_get_property;
  object_class->set_property = gbp_arduino_string_row_set_property;

  properties[PROP_LIBRARY_NAME] =
      g_param_spec_string ("library-name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[REMOVE] =
      g_signal_new ("remove",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE,
                    0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/plugins/arduino/gbp-arduino-string-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpArduinoStringRow, box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoStringRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoStringRow, remove_button);
}

static void
gbp_arduino_string_row_init (GbpArduinoStringRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->remove_button,
                    "clicked",
                    G_CALLBACK (remove_button_clicked),
                    self);
}

GtkWidget *
gbp_arduino_string_row_new (const char *library_name)
{
  return g_object_new (GBP_TYPE_ARDUINO_STRING_ROW,
                       "library-name", library_name,
                       NULL);
}

const char *
gbp_arduino_string_row_get_name (GbpArduinoStringRow *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_STRING_ROW (self), NULL);

  return self->library_name;
}

