/*
 * gbp-arduino-platform-editor-row.c
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

#define G_LOG_DOMAIN "gbp-arduino-platform-editor-row"

#include "config.h"

#include "gbp-arduino-platform-editor-row.h"

struct _GbpArduinoPlatformEditorRow
{
  GtkListBoxRow parent_instance;

  GbpArduinoPlatform *platform;

  GtkBox    *box;
  GtkLabel  *name_label;
  GtkLabel  *version_label;
  GtkButton *remove_button;
};

enum
{
  PROP_0,
  PROP_PLATFORM,
  N_PROPS
};

enum
{
  REMOVE,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (GbpArduinoPlatformEditorRow, gbp_arduino_platform_editor_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
remove_button_clicked (GtkButton *button,
                      GbpArduinoPlatformEditorRow *self)
{
  g_assert (GBP_IS_ARDUINO_PLATFORM_EDITOR_ROW (self));
  g_assert (GTK_IS_BUTTON (button));

  g_signal_emit (self, signals[REMOVE], 0);
}

static void
gbp_arduino_platform_editor_row_dispose (GObject *object)
{
  GbpArduinoPlatformEditorRow *self = (GbpArduinoPlatformEditorRow *) object;

  g_clear_pointer ((GtkWidget **) &self->box, gtk_widget_unparent);
  g_clear_object (&self->platform);

  G_OBJECT_CLASS (gbp_arduino_platform_editor_row_parent_class)->dispose (object);
}

static void
gbp_arduino_platform_editor_row_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
  GbpArduinoPlatformEditorRow *self = GBP_ARDUINO_PLATFORM_EDITOR_ROW (object);

  switch (prop_id)
    {
    case PROP_PLATFORM:
      g_value_set_object (value, self->platform);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_platform_editor_row_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
  GbpArduinoPlatformEditorRow *self = GBP_ARDUINO_PLATFORM_EDITOR_ROW (object);

  switch (prop_id)
    {
    case PROP_PLATFORM:
      g_set_object (&self->platform, g_value_get_object (value));
      if (self->name_label != NULL)
        gtk_label_set_text (self->name_label,
                           gbp_arduino_platform_get_name (self->platform));
      if (self->version_label != NULL)
        gtk_label_set_text (self->version_label,
                           gbp_arduino_platform_get_version (self->platform));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_platform_editor_row_class_init (GbpArduinoPlatformEditorRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_arduino_platform_editor_row_dispose;
  object_class->get_property = gbp_arduino_platform_editor_row_get_property;
  object_class->set_property = gbp_arduino_platform_editor_row_set_property;

  properties[PROP_PLATFORM] =
      g_param_spec_object ("platform", NULL, NULL,
                          GBP_TYPE_ARDUINO_PLATFORM,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

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
                                             "/plugins/arduino/gbp-arduino-platform-editor-row.ui");

  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformEditorRow, box);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformEditorRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformEditorRow, version_label);
  gtk_widget_class_bind_template_child (widget_class, GbpArduinoPlatformEditorRow, remove_button);
}

static void
gbp_arduino_platform_editor_row_init (GbpArduinoPlatformEditorRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->remove_button,
                    "clicked",
                    G_CALLBACK (remove_button_clicked),
                    self);
}

GtkWidget *
gbp_arduino_platform_editor_row_new (GbpArduinoPlatform *platform)
{
  return g_object_new (GBP_TYPE_ARDUINO_PLATFORM_EDITOR_ROW,
                      "platform", platform,
                      NULL);
}

GbpArduinoPlatform *
gbp_arduino_platform_editor_row_get_platform (GbpArduinoPlatformEditorRow *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_PLATFORM_EDITOR_ROW (self), NULL);
  return self->platform;
}
