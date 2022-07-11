/* gstyle-cielab.c
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

#define G_LOG_DOMAIN "gstyle-cielab"

#include "gstyle-cielab.h"

G_DEFINE_BOXED_TYPE (GstyleCielab, gstyle_cielab, gstyle_cielab_copy, gstyle_cielab_free)

/**
 * GstyleCielab:
 * @l: color- lightness dimension from 0 (darkest black) to 100 (brightest white)
 * @a: color-opponent dimension from green (-300) to red (+299).
 * @b: color-opponent dimension from blue (-300) to yellow (+299).
 * @alpha: The opacity of the color in [0, 1] range.
 *
 * A #GstyleCielab used to represent a color in
 * the CIE L*a*b* 1976 colorspace.
 */

/**
 * gstyle_cielab_copy:
 * @self: a #GstyleCielab
 *
 * Makes a copy of a #GstyleCielab.
 *
 * The result must be freed through gstyle_cielab_free().
 *
 * Returns: a newly allocated #GstyleCielab, with the same contents as @self
 *
 */
GstyleCielab *
gstyle_cielab_copy (const GstyleCielab *self)
{
  return g_slice_dup (GstyleCielab, self);
}

/**
 * gstyle_cielab_free:
 * @self: a #GstyleCielab
 *
 * Frees a #GstyleCielab created with gstyle_cielab_copy().
 *
 */
void
gstyle_cielab_free (GstyleCielab *self)
{
  g_slice_free (GstyleCielab, self);
}
