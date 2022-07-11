/* gstyle-color-component.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define GSTYLE_TYPE_COLOR_COMPONENT (gstyle_color_component_get_type()

typedef enum {
  GSTYLE_COLOR_COMPONENT_HSV_H,
  GSTYLE_COLOR_COMPONENT_HSV_S,
  GSTYLE_COLOR_COMPONENT_HSV_V,
  GSTYLE_COLOR_COMPONENT_LAB_L,
  GSTYLE_COLOR_COMPONENT_LAB_A,
  GSTYLE_COLOR_COMPONENT_LAB_B,
  GSTYLE_COLOR_COMPONENT_RGB_RED,
  GSTYLE_COLOR_COMPONENT_RGB_GREEN,
  GSTYLE_COLOR_COMPONENT_RGB_BLUE,
  N_GSTYLE_COLOR_COMPONENT,
  GSTYLE_COLOR_COMPONENT_NONE
} GstyleColorComponent;

GType gstyle_color_component_get_type (void);

G_END_DECLS
