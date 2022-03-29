/* ide-notification-view-private.h
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

#include <adwaita.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_NOTIFICATION_VIEW (ide_notification_view_get_type())

G_DECLARE_FINAL_TYPE (IdeNotificationView, ide_notification_view, IDE, NOTIFICATION_VIEW, AdwBin)

GtkWidget       *ide_notification_view_new              (void);
IdeNotification *ide_notification_view_get_notification (IdeNotificationView *self);
void             ide_notification_view_set_notification (IdeNotificationView *self,
                                                         IdeNotification     *notification);

G_END_DECLS
