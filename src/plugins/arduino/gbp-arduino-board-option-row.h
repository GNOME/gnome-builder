/*
 * gbp-arduino-board-option-row.h
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

#include "gbp-arduino-board-option.h"
#include <adwaita.h>

G_BEGIN_DECLS

#define GBP_TYPE_ARDUINO_BOARD_OPTION_ROW (gbp_arduino_board_option_row_get_type ())

G_DECLARE_FINAL_TYPE (GbpArduinoBoardOptionRow, gbp_arduino_board_option_row, GBP, ARDUINO_BOARD_OPTION_ROW, AdwComboRow)

GtkWidget *gbp_arduino_board_option_row_new (GbpArduinoBoardOption *option);

GbpArduinoBoardOption *gbp_arduino_board_option_row_get_option (GbpArduinoBoardOptionRow *self);

G_END_DECLS

