/*
 * gbp-arduino-option-value.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_ARDUINO_OPTION_VALUE (gbp_arduino_option_value_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoOptionValue, gbp_arduino_option_value, GBP, ARDUINO_OPTION_VALUE, GObject)

GbpArduinoOptionValue *gbp_arduino_option_value_new (const char *value,
                                                     const char *value_label);

const char *gbp_arduino_option_value_get_value (GbpArduinoOptionValue *self);

const char *gbp_arduino_option_value_get_value_label (GbpArduinoOptionValue *self);

G_END_DECLS

