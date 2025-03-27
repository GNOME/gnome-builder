/*
 * gbp-arduino-board-option.c
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

#define G_LOG_DOMAIN "gbp-arduino-board-option"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-arduino-board-option.h"
#include "gbp-arduino-option-value.h"

enum
{
  PROP_0,
  PROP_OPTION,
  PROP_OPTION_LABEL,
  N_PROPS
};

struct _GbpArduinoBoardOption
{
  GObject parent_instance;

  char       *option;
  char       *option_label;
  GListStore *values;
};

G_DEFINE_TYPE (GbpArduinoBoardOption, gbp_arduino_board_option, G_TYPE_OBJECT)

static GParamSpec *obj_properties[N_PROPS];

static void
gbp_arduino_board_option_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpArduinoBoardOption *self = GBP_ARDUINO_BOARD_OPTION (object);

  switch (prop_id)
    {
    case PROP_OPTION:
      g_set_str (&self->option, g_value_get_string (value));
      break;
    case PROP_OPTION_LABEL:
      g_set_str (&self->option_label, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gbp_arduino_board_option_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpArduinoBoardOption *self = GBP_ARDUINO_BOARD_OPTION (object);

  switch (prop_id)
    {
    case PROP_OPTION:
      g_value_set_string (value, self->option);
      break;
    case PROP_OPTION_LABEL:
      g_value_set_string (value, self->option_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gbp_arduino_board_option_finalize (GObject *object)
{
  GbpArduinoBoardOption *self = GBP_ARDUINO_BOARD_OPTION (object);

  g_clear_pointer (&self->option, g_free);
  g_clear_pointer (&self->option_label, g_free);
  g_clear_object (&self->values);

  G_OBJECT_CLASS (gbp_arduino_board_option_parent_class)->finalize (object);
}

static void
gbp_arduino_board_option_class_init (GbpArduinoBoardOptionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_board_option_finalize;
  object_class->set_property = gbp_arduino_board_option_set_property;
  object_class->get_property = gbp_arduino_board_option_get_property;

  obj_properties[PROP_OPTION] =
      g_param_spec_string ("option", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_OPTION_LABEL] =
      g_param_spec_string ("option-label", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, obj_properties);
}

static void
gbp_arduino_board_option_init (GbpArduinoBoardOption *self)
{
  self->values = g_list_store_new (G_TYPE_OBJECT);
}

GbpArduinoBoardOption *
gbp_arduino_board_option_new (const char *option,
                              const char *option_label)
{
  return g_object_new (GBP_TYPE_ARDUINO_BOARD_OPTION,
                       "option", option,
                       "option-label", option_label,
                       NULL);
}

void
gbp_arduino_board_option_add_value (GbpArduinoBoardOption *self,
                                    const char            *value,
                                    const char            *value_label)
{
  g_autoptr (GbpArduinoOptionValue) option_value = NULL;

  g_return_if_fail (GBP_IS_ARDUINO_BOARD_OPTION (self));

  option_value = gbp_arduino_option_value_new (value, value_label);

  g_list_store_append (self->values, option_value);
}

GListStore *
gbp_arduino_board_option_get_values (GbpArduinoBoardOption *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD_OPTION (self), NULL);
  return self->values;
}

const char *
gbp_arduino_board_option_get_option (GbpArduinoBoardOption *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD_OPTION (self), NULL);
  return self->option;
}

const char *
gbp_arduino_board_option_get_option_label (GbpArduinoBoardOption *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD_OPTION (self), NULL);
  return self->option_label;
}

