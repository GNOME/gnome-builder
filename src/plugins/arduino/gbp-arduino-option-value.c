/*
 * gbp-arduino-option-value.c
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

#define G_LOG_DOMAIN "gbp-arduino-option-value"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-arduino-option-value.h"

struct _GbpArduinoOptionValue
{
  GObject parent_instance;

  char *value;
  char *value_label;
};

enum
{
  PROP_0,
  PROP_VALUE,
  PROP_VALUE_LABEL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpArduinoOptionValue, gbp_arduino_option_value, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_option_value_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpArduinoOptionValue *self = GBP_ARDUINO_OPTION_VALUE (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_set_str (&self->value, g_value_get_string (value));
      break;
    case PROP_VALUE_LABEL:
      g_set_str (&self->value_label, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gbp_arduino_option_value_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpArduinoOptionValue *self = GBP_ARDUINO_OPTION_VALUE (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_string (value, self->value);
      break;
    case PROP_VALUE_LABEL:
      g_value_set_string (value, self->value_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gbp_arduino_option_value_finalize (GObject *object)
{
  GbpArduinoOptionValue *self = GBP_ARDUINO_OPTION_VALUE (object);

  g_clear_pointer (&self->value, g_free);
  g_clear_pointer (&self->value_label, g_free);

  G_OBJECT_CLASS (gbp_arduino_option_value_parent_class)->finalize (object);
}

static void
gbp_arduino_option_value_class_init (GbpArduinoOptionValueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_option_value_finalize;
  object_class->set_property = gbp_arduino_option_value_set_property;
  object_class->get_property = gbp_arduino_option_value_get_property;

  properties[PROP_VALUE] =
      g_param_spec_string ("value", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_VALUE_LABEL] =
      g_param_spec_string ("value-label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_option_value_init (GbpArduinoOptionValue *self)
{
}

GbpArduinoOptionValue *
gbp_arduino_option_value_new (const char *value,
                              const char *value_label)
{
  return g_object_new (GBP_TYPE_ARDUINO_OPTION_VALUE,
                       "value", value,
                       "value-label", value_label,
                       NULL);
}

const char *
gbp_arduino_option_value_get_value (GbpArduinoOptionValue *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_OPTION_VALUE (self), NULL);
  return (const char *) self->value;
}

const char *
gbp_arduino_option_value_get_value_label (GbpArduinoOptionValue *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_OPTION_VALUE (self), NULL);
  return (const char *) self->value_label;
}

