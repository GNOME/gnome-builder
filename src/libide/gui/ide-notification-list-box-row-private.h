/* ide-notification-list-box-row-private.h
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

#pragma once

#include <libide-core.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_NOTIFICATION_LIST_BOX_ROW (ide_notification_list_box_row_get_type())

G_DECLARE_FINAL_TYPE (IdeNotificationListBoxRow, ide_notification_list_box_row, IDE, NOTIFICATION_LIST_BOX_ROW, GtkListBoxRow)

GtkWidget       *ide_notification_list_box_row_new              (IdeNotification           *notification);
IdeNotification *ide_notification_list_box_row_get_notification (IdeNotificationListBoxRow *self);
void             ide_notification_list_box_row_set_compact      (IdeNotificationListBoxRow *self,
                                                                 gboolean                   compact);
gboolean         ide_notification_list_box_row_get_compact      (IdeNotificationListBoxRow *self);

G_END_DECLS
