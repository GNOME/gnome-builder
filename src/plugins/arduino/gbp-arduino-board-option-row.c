/*
 * gbp-arduino-board-option-row.c
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

#define G_LOG_DOMAIN "gbp-arduino-board-option-row"

#include "config.h"

#include "gbp-arduino-board-option-row.h"
#include "gbp-arduino-option-value.h"

struct _GbpArduinoBoardOptionRow
{
  AdwComboRow parent_instance;

  GbpArduinoBoardOption *option;
};

G_DEFINE_FINAL_TYPE (GbpArduinoBoardOptionRow, gbp_arduino_board_option_row, ADW_TYPE_COMBO_ROW)

enum
{
  PROP_0,
  PROP_OPTION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gpointer
map_option_cb (gpointer item,
               gpointer user_data)
{
  g_autoptr (GbpArduinoOptionValue) option_value = item;
  return gtk_string_object_new (gbp_arduino_option_value_get_value_label (option_value));
}

static void
gbp_arduino_board_option_row_finalize (GObject *object)
{
  GbpArduinoBoardOptionRow *self = GBP_ARDUINO_BOARD_OPTION_ROW (object);

  g_clear_object (&self->option);

  G_OBJECT_CLASS (gbp_arduino_board_option_row_parent_class)->finalize (object);
}

static void
gbp_arduino_board_option_row_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
  GbpArduinoBoardOptionRow *self = GBP_ARDUINO_BOARD_OPTION_ROW (object);

  switch (prop_id)
    {
    case PROP_OPTION:
      g_value_set_object (value, self->option);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_board_option_row_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
  GbpArduinoBoardOptionRow *self = GBP_ARDUINO_BOARD_OPTION_ROW (object);

  switch (prop_id)
    {
    case PROP_OPTION:
      self->option = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_arduino_board_option_row_class_init (GbpArduinoBoardOptionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_arduino_board_option_row_finalize;
  object_class->get_property = gbp_arduino_board_option_row_get_property;
  object_class->set_property = gbp_arduino_board_option_row_set_property;

  properties[PROP_OPTION] =
      g_param_spec_object ("option", NULL, NULL,
                           GBP_TYPE_ARDUINO_BOARD_OPTION,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_arduino_board_option_row_init (GbpArduinoBoardOptionRow *self)
{
}

GtkWidget *
gbp_arduino_board_option_row_new (GbpArduinoBoardOption *option)
{
  g_autoptr(GtkMapListModel) mapped = NULL;
  GListModel *model;

  GtkWidget *widget = g_object_new (GBP_TYPE_ARDUINO_BOARD_OPTION_ROW,
                                    "option", option,
                                    "title", gbp_arduino_board_option_get_option_label (option),
                                    NULL);

  model = G_LIST_MODEL (gbp_arduino_board_option_get_values (option));
  mapped = gtk_map_list_model_new (g_object_ref (model),
                                   map_option_cb,
                                   NULL,
                                   NULL);

  adw_combo_row_set_model (ADW_COMBO_ROW (widget), G_LIST_MODEL (mapped));

  return widget;
}

GbpArduinoBoardOption *
gbp_arduino_board_option_row_get_option (GbpArduinoBoardOptionRow *self)
{
  g_return_val_if_fail (GBP_IS_ARDUINO_BOARD_OPTION_ROW (self), NULL);
  return self->option;
}

