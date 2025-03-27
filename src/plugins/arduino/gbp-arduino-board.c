/*
 * gbp-arduino-board.c
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

#define G_LOG_DOMAIN "gbp-arduino-board"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-arduino-board.h"

struct _GbpArduinoBoard
{
  GObject parent_instance;

  char *platform;
  char *name;
  char *fqbn;
};

G_DEFINE_FINAL_TYPE (GbpArduinoBoard, gbp_arduino_board, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_PLATFORM,
  PROP_NAME,
  PROP_FQBN,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gbp_arduino_board_finalize (GObject *object)
{
  GbpArduinoBoard *self = GBP_ARDUINO_BOARD (object);

  g_clear_pointer (&self->platform, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->fqbn, g_free);

  G_OBJECT_CLASS (gbp_arduino_board_parent_class)->finalize (object);
}

static void
gbp_arduino_board_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbpArduinoBoard *self = GBP_ARDUINO_BOARD (object);

  switch (prop_id)
    {
    case PROP_PLATFORM:
      g_value_set_string (value, self->name);
      break;
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case PROP_FQBN:
      g_value_set_string (value, self->fqbn);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_board_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbpArduinoBoard *self = GBP_ARDUINO_BOARD (object);

  switch (prop_id)
    {
    case PROP_PLATFORM:
      g_set_str (&self->platform, g_value_get_string (value));
      break;
    case PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;
    case PROP_FQBN:
      g_set_str (&self->fqbn, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_board_class_init (GbpArduinoBoardClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_board_finalize;
  object_class->get_property = gbp_arduino_board_get_property;
  object_class->set_property = gbp_arduino_board_set_property;

  properties[PROP_PLATFORM] =
      g_param_spec_string ("platform", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FQBN] =
      g_param_spec_string ("fqbn", NULL, NULL,
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_board_init (GbpArduinoBoard *self)
{
}

GbpArduinoBoard *
gbp_arduino_board_new (const char *platform,
                       const char *name,
                       const char *fqbn)
{
  return g_object_new (GBP_TYPE_ARDUINO_BOARD,
                       "name", name,
                       "fqbn", fqbn,
                       "platform", platform,
                       NULL);
}

const char *
gbp_arduino_board_get_platform (GbpArduinoBoard *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD (self), NULL);
  return self->platform;
}

const char *
gbp_arduino_board_get_name (GbpArduinoBoard *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD (self), NULL);
  return self->name;
}

const char *
gbp_arduino_board_get_fqbn (GbpArduinoBoard *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD (self), NULL);
  return self->fqbn;
}

