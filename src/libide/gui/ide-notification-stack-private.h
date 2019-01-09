/* ide-notification-stack-private.h
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

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_NOTIFICATION_STACK (ide_notification_stack_get_type())

G_DECLARE_FINAL_TYPE (IdeNotificationStack, ide_notification_stack, IDE, NOTIFICATION_STACK, GtkStack)

GtkWidget       *ide_notification_stack_new           (void);
void             ide_notification_stack_bind_model    (IdeNotificationStack *self,
                                                       GListModel           *notifications);
gboolean         ide_notification_stack_is_empty      (IdeNotificationStack *self);
gboolean         ide_notification_stack_get_can_move  (IdeNotificationStack *self);
void             ide_notification_stack_move_next     (IdeNotificationStack *self);
void             ide_notification_stack_move_previous (IdeNotificationStack *self);
IdeNotification *ide_notification_stack_get_visible   (IdeNotificationStack *self);
gdouble          ide_notification_stack_get_progress  (IdeNotificationStack *self);
void             ide_notification_stack_set_progress  (IdeNotificationStack *self,
                                                       gdouble               progress);

G_END_DECLS
