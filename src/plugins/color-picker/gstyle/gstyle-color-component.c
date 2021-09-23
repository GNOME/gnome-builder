/* gstyle-color-component.c
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

#include <gtk/gtk.h>

#include "gstyle-color-component.h"

GType
gstyle_color_component_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_COMPONENT_HSV_H,     "GSTYLE_COLOR_COMPONENT_HSV_H",     "hsv_h"    },
    { GSTYLE_COLOR_COMPONENT_HSV_S,     "GSTYLE_COLOR_COMPONENT_HSV_S",     "hsv_s"   },
    { GSTYLE_COLOR_COMPONENT_HSV_V,     "GSTYLE_COLOR_COMPONENT_HSV_V",     "hsv_v"  },
    { GSTYLE_COLOR_COMPONENT_LAB_L,     "GSTYLE_COLOR_COMPONENT_LAB_L",     "lab_l"    },
    { GSTYLE_COLOR_COMPONENT_LAB_A,     "GSTYLE_COLOR_COMPONENT_LAB_A",     "lab_a"  },
    { GSTYLE_COLOR_COMPONENT_LAB_B,     "GSTYLE_COLOR_COMPONENT_LAB_B",     "lab_b"   },
    { GSTYLE_COLOR_COMPONENT_RGB_RED,   "GSTYLE_COLOR_COMPONENT_RGB_RED",   "rgb_red" },
    { GSTYLE_COLOR_COMPONENT_RGB_GREEN, "GSTYLE_COLOR_COMPONENT_RGB_GREEN", "rgb_green" },
    { GSTYLE_COLOR_COMPONENT_RGB_BLUE,  "GSTYLE_COLOR_COMPONENT_RGB_BLUE",  "rgb_blue" },
    { GSTYLE_COLOR_COMPONENT_NONE,      "GSTYLE_COLOR_COMPONENT_NONE",      "none" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorComponent", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
