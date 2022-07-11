/* ide-notification.h
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

G_BEGIN_DECLS

#define IDE_TYPE_NOTIFICATION (ide_notification_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeNotification, ide_notification, IDE, NOTIFICATION, IdeObject)

struct _IdeNotificationClass
{
  IdeObjectClass parent_class;

  /*< private */
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeNotification *ide_notification_new                                 (void);
IDE_AVAILABLE_IN_ALL
void             ide_notification_attach                              (IdeNotification  *self,
                                                                       IdeObject        *object);
IDE_AVAILABLE_IN_ALL
gchar           *ide_notification_dup_id                              (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_id                              (IdeNotification  *self,
                                                                       const gchar      *id);
IDE_AVAILABLE_IN_ALL
gchar           *ide_notification_dup_title                           (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_title                           (IdeNotification  *self,
                                                                       const gchar      *title);
IDE_AVAILABLE_IN_ALL
GIcon           *ide_notification_ref_icon                            (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_icon                            (IdeNotification  *self,
                                                                       GIcon            *icon);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_icon_name                       (IdeNotification  *self,
                                                                       const gchar      *icon_name);
IDE_AVAILABLE_IN_ALL
gchar           *ide_notification_dup_body                            (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_body                            (IdeNotification  *self,
                                                                       const gchar      *body);
IDE_AVAILABLE_IN_ALL
gboolean         ide_notification_get_has_progress                    (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_has_progress                    (IdeNotification  *self,
                                                                       gboolean          has_progress);
IDE_AVAILABLE_IN_ALL
gint             ide_notification_get_priority                        (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_priority                        (IdeNotification  *self,
                                                                       gint              priority);
IDE_AVAILABLE_IN_ALL
gdouble          ide_notification_get_progress                        (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_progress                        (IdeNotification  *self,
                                                                       gdouble           progress);
IDE_AVAILABLE_IN_ALL
gboolean         ide_notification_get_progress_is_imprecise           (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_progress_is_imprecise           (IdeNotification  *self,
                                                                       gboolean          progress_is_imprecise);
IDE_AVAILABLE_IN_ALL
gboolean         ide_notification_get_urgent                          (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_urgent                          (IdeNotification  *self,
                                                                       gboolean          urgent);
IDE_AVAILABLE_IN_ALL
guint            ide_notification_get_n_buttons                       (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
gboolean         ide_notification_get_button                          (IdeNotification  *self,
                                                                       guint             button,
                                                                       gchar           **label,
                                                                       GIcon           **icon,
                                                                       gchar           **action,
                                                                       GVariant        **target);
IDE_AVAILABLE_IN_ALL
void             ide_notification_add_button                          (IdeNotification  *self,
                                                                       const gchar      *label,
                                                                       GIcon            *icon,
                                                                       const gchar      *detailed_action);
IDE_AVAILABLE_IN_ALL
void             ide_notification_add_button_with_target_value        (IdeNotification  *self,
                                                                       const gchar      *label,
                                                                       GIcon            *icon,
                                                                       const gchar      *action,
                                                                       GVariant         *target);
IDE_AVAILABLE_IN_ALL
gboolean         ide_notification_get_default_action                  (IdeNotification  *self,
                                                                       gchar           **action,
                                                                       GVariant        **target);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_default_action                  (IdeNotification  *self,
                                                                       const gchar      *detailed_action);
IDE_AVAILABLE_IN_ALL
void             ide_notification_set_default_action_and_target_value (IdeNotification  *self,
                                                                       const gchar      *action,
                                                                       GVariant         *target);
IDE_AVAILABLE_IN_ALL
gint             ide_notification_compare                             (IdeNotification  *a,
                                                                       IdeNotification  *b);
IDE_AVAILABLE_IN_ALL
void             ide_notification_withdraw                            (IdeNotification  *self);
IDE_AVAILABLE_IN_ALL
void             ide_notification_withdraw_in_seconds                 (IdeNotification  *self,
                                                                       gint              seconds);
IDE_AVAILABLE_IN_ALL
void             ide_notification_file_progress_callback              (goffset           current_num_bytes,
                                                                       goffset           total_num_bytes,
                                                                       gpointer          user_data);
IDE_AVAILABLE_IN_ALL
void             ide_notification_flatpak_progress_callback           (const char       *status,
                                                                       guint             notification,
                                                                       gboolean          estimating,
                                                                       gpointer          user_data);


G_END_DECLS
