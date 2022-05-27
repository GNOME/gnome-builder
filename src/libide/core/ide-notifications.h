/* ide-notifications.h
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

#include "ide-object.h"
#include "ide-notification.h"

G_BEGIN_DECLS

#define IDE_TYPE_NOTIFICATIONS (ide_notifications_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeNotifications, ide_notifications, IDE, NOTIFICATIONS, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeNotifications *ide_notifications_new                       (void);
IDE_AVAILABLE_IN_ALL
void              ide_notifications_add_notification          (IdeNotifications *self,
                                                               IdeNotification  *notification);
IDE_AVAILABLE_IN_ALL
gdouble           ide_notifications_get_progress              (IdeNotifications *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_notifications_get_has_progress          (IdeNotifications *self);
IDE_AVAILABLE_IN_ALL
gboolean          ide_notifications_get_progress_is_imprecise (IdeNotifications *self);
IDE_AVAILABLE_IN_ALL
IdeNotification  *ide_notifications_find_by_id                (IdeNotifications *self,
                                                               const gchar      *id);

G_END_DECLS
