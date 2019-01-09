/* ide-notifications-button-popover.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-notifications-button-popover"

#include "config.h"

#include "ide-notifications-button-popover-private.h"

struct _IdeNotificationsButtonPopover
{
  GtkPopover parent_instance;
};

G_DEFINE_TYPE (IdeNotificationsButtonPopover, ide_notifications_button_popover, GTK_TYPE_POPOVER)

static GtkSizeRequestMode
ide_notifications_button_popover_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
ide_notifications_button_popover_class_init (IdeNotificationsButtonPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->get_request_mode = ide_notifications_button_popover_get_request_mode;
}

static void
ide_notifications_button_popover_init (IdeNotificationsButtonPopover *self)
{
}
