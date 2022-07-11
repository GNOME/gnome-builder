/* gstyle-animation.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include "gstyle-animation.h"

gdouble
gstyle_animation_ease_in_out_cubic (gdouble offset)
{
  gdouble val;

  if (offset < 0.5)
    return (offset * offset * offset * 4.0);
  else
    {
      val = (offset - 0.5) * 2.0;
      val = val - 1.0;
      val = val * val * val + 1.0;
      val = val / 2.0 + 0.5;

      return val;
    }
}

gboolean
gstyle_animation_check_enable_animation (void)
{
  gboolean enable_animation;

  g_object_get (gtk_settings_get_default (),
                "gtk-enable-animations", &enable_animation,
                NULL);

  return enable_animation;
}
