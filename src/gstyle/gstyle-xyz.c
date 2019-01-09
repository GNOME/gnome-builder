/* gstyle-xyz.c
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

#define G_LOG_DOMAIN "gstyle-xyz"

#include "gstyle-xyz.h"

G_DEFINE_BOXED_TYPE (GstyleXYZ, gstyle_xyz, gstyle_xyz_copy, gstyle_xyz_free)

/**
 * GstyleXYZ:
 * @x: Tristimulus X value.
 * @y: Tristimulus Y value.
 * @z: Tristimulus Z value.
 * @alpha: The opacity of the color in [0, 1] range.
 *
 * A #GstyleXYZ is used to represent a color in
 * the CIE 1931 XYZ color space.
 */

/**
 * gstyle_xyz_copy:
 * @self: a #GstyleXYZ
 *
 * Makes a copy of a #GstyleXYZ.
 *
 * The result must be freed through gstyle_xyz_free().
 *
 * Returns: a newly allocated #GstyleXYZ with the same content as @self.
 *
 */
GstyleXYZ *
gstyle_xyz_copy (const GstyleXYZ *self)
{
  return g_slice_dup (GstyleXYZ, self);
}

/**
 * gstyle_xyz_free:
 * @self: a #GstyleXYZ
 *
 * Frees a #GstyleXYZ created with gstyle_xyz_copy().
 *
 */
void
gstyle_xyz_free (GstyleXYZ *self)
{
  g_slice_free (GstyleXYZ, self);
}
