/* ide-popover-positioner.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-popover-positioner"

#include "config.h"

#include "ide-popover-positioner.h"

G_DEFINE_INTERFACE (IdePopoverPositioner, ide_popover_positioner, GTK_TYPE_WIDGET)

static void
ide_popover_positioner_default_init (IdePopoverPositionerInterface *iface)
{
}

void
ide_popover_positioner_present (IdePopoverPositioner *self,
                                GtkPopover           *popover,
                                GtkWidget            *relative_to,
                                const GdkRectangle   *pointing_at)
{
  GdkRectangle empty;

  g_return_if_fail (IDE_IS_POPOVER_POSITIONER (self));
  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (GTK_IS_WIDGET (relative_to));

  if (pointing_at == NULL)
    {
      empty.x = 0;
      empty.y = 0;
      empty.width = gtk_widget_get_width (relative_to);
      empty.height = gtk_widget_get_height (relative_to);
      pointing_at = &empty;
    }

  IDE_POPOVER_POSITIONER_GET_IFACE (self)->present (self, popover, relative_to, pointing_at);
}
