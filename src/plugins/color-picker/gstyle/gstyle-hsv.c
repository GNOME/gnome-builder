/* gstyle-hsv.c
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

#define G_LOG_DOMAIN "gstyle-hsv"

#include "gstyle-hsv.h"

G_DEFINE_BOXED_TYPE (GstyleHSV, gstyle_hsv, gstyle_hsv_copy, gstyle_hsv_free)

/**
 * GstyleHSV:
 * @h: color hue in the range [0, 360[ degrees.
 * @s: color saturation in the range [0, 1].
 * @v: color value in the range [0, 1].
 * @alpha: The opacity of the color in [0, 1] range.
 *
 * A #GstyleHSV is used to represent a color in
 * the HSV (also called HSB) colorspace.
 */

/**
 * gstyle_hsv_copy:
 * @self: a #GstyleHSV
 *
 * Makes a copy of a #GstyleHSV
 *
 * The result must be freed through gstyle_hsv_free().
 *
 * Returns: a newly allocated #GstyleHSV with the same content as @self.
 *
 */
GstyleHSV *
gstyle_hsv_copy (const GstyleHSV *self)
{
  return g_slice_dup (GstyleHSV, self);
}

/**
 * gstyle_hsv_free:
 * @self: a #GstyleHSV
 *
 * Frees a #GstyleHSV created with gstyle_hsv_copy().
 *
 */
void
gstyle_hsv_free (GstyleHSV *self)
{
  g_slice_free (GstyleHSV, self);
}
