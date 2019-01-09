/* gstyle-color-convert.h
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
#include <gdk/gdk.h>

#include "gstyle-types.h"
#include "gstyle-cielab.h"
#include "gstyle-hsv.h"
#include "gstyle-xyz.h"

G_BEGIN_DECLS

void                  gstyle_color_convert_rgb_to_hsl       (GdkRGBA             *rgba,
                                                             gdouble             *hue,
                                                             gdouble             *saturation,
                                                             gdouble             *lightness);
void                  gstyle_color_convert_rgb_to_hsv       (GdkRGBA             *rgba,
                                                             gdouble             *hue,
                                                             gdouble             *saturation,
                                                             gdouble             *value);
void                  gstyle_color_convert_hsl_to_rgb       (gdouble              hue,
                                                             gdouble              saturation,
                                                             gdouble              lightness,
                                                             GdkRGBA             *rgba);
void                  gstyle_color_convert_hsv_to_rgb       (gdouble              hue,
                                                             gdouble              saturation,
                                                             gdouble              value,
                                                             GdkRGBA             *rgba);
void                  gstyle_color_convert_rgb_to_cielab    (GdkRGBA             *rgba,
                                                             GstyleCielab        *lab);
void                  gstyle_color_convert_cielab_to_rgb    (GstyleCielab        *lab,
                                                             GdkRGBA             *rgba);
gdouble               gstyle_color_delta_e                  (GstyleCielab        *lab1,
                                                             GstyleCielab        *lab2);

void                  gstyle_color_convert_rgb_to_xyz       (GdkRGBA             *rgba,
                                                             GstyleXYZ           *xyz);
extern void           gstyle_color_convert_cielab_to_xyz    (GstyleCielab        *lab,
                                                             GstyleXYZ           *xyz);
void                  gstyle_color_convert_hsv_to_xyz       (gdouble              hue,
                                                             gdouble              saturation,
                                                             gdouble              value,
                                                             GstyleXYZ           *xyz);

extern void           gstyle_color_convert_xyz_to_cielab    (GstyleXYZ           *xyz,
                                                             GstyleCielab        *lab);
extern void           gstyle_color_convert_xyz_to_rgb       (GstyleXYZ           *xyz,
                                                             GdkRGBA             *rgba);
void                  gstyle_color_convert_xyz_to_hsv       (GstyleXYZ           *xyz,
                                                             gdouble             *hue,
                                                             gdouble             *saturation,
                                                             gdouble             *value);

G_END_DECLS
