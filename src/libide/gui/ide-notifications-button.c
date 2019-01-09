/* ide-notifications-button.c
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

#define G_LOG_DOMAIN "ide-notifications-button"

#include "config.h"

#include "ide-notifications-button.h"
#include "ide-gui-global.h"
#include "ide-gui-private.h"

/**
 * SECTION:ide-notifications-button:
 * @title: IdeNotificationsButton
 * @short_description: a popover menu button containing progress notifications
 *
 * The #IdeNotificationsButton shows ongoing notifications that have progress.
 * The individual notifications are displayed in a popover with appropriate
 * progress show for each.
 *
 * The button itself will show a "combined" progress of all the active
 * notifications.
 *
 * Since: 3.32
 */

struct _IdeNotificationsButton
{
  DzlProgressMenuButton  parent_instance;

  GListModel            *model;
  DzlListModelFilter    *filter;

  /* Template widgets */
  GtkPopover            *popover;
  GtkListBox            *list_box;
};

G_DEFINE_TYPE (IdeNotificationsButton, ide_notifications_button, DZL_TYPE_PROGRESS_MENU_BUTTON)

static GtkWidget *
create_notification_row (gpointer item,
                         gpointer user_data)
{
  IdeNotification *notif = item;
  gboolean has_default;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (user_data));

  has_default = ide_notification_get_default_action (notif, NULL, NULL);

  return g_object_new (IDE_TYPE_NOTIFICATION_LIST_BOX_ROW,
                       "activatable", has_default,
                       "compact", TRUE,
                       "notification", item,
                       "visible", TRUE,
                       NULL);
}

static gboolean
filter_by_has_progress (GObject  *object,
                        gpointer  user_data)
{
  IdeNotification *notif = (IdeNotification *)object;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (user_data == NULL);

  return ide_notification_get_has_progress (notif);
}

static void
ide_notifications_button_bind_model (IdeNotificationsButton *self,
                                     GListModel             *model)
{
  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (g_set_object (&self->model, model))
    {
      g_clear_object (&self->filter);

      self->filter = dzl_list_model_filter_new (model);
      dzl_list_model_filter_set_filter_func (self->filter,
                                             filter_by_has_progress,
                                             NULL, NULL);

      gtk_list_box_bind_model (self->list_box,
                               G_LIST_MODEL (self->filter),
                               create_notification_row,
                               self, NULL);
    }
}

static void
ide_notifications_button_context_set_cb (GtkWidget  *widget,
                                         IdeContext *context)
{
  IdeNotificationsButton *self = (IdeNotificationsButton *)widget;
  g_autoptr(IdeNotifications) notifications = NULL;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (IDE_IS_CONTEXT (context));

  notifications = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
  ide_notifications_button_bind_model (self, G_LIST_MODEL (notifications));

  g_object_bind_property (notifications, "progress", self, "progress",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (notifications, "has-progress", self, "visible",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (notifications, "progress-is-imprecise", self, "show-progress",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
}

static void
ide_notifications_button_row_activated (IdeNotificationsButton    *self,
                                        IdeNotificationListBoxRow *row,
                                        GtkListBox                *list_box)
{
  g_autofree gchar *default_action = NULL;
  g_autoptr(GVariant) default_target = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  notif = ide_notification_list_box_row_get_notification (row);

  if (ide_notification_get_default_action (notif, &default_action, &default_target))
    {
      gchar *name = strchr (default_action, '.');
      gchar *group = default_action;

      if (name != NULL)
        {
          *name = '\0';
          name++;
        }
      else
        {
          group = NULL;
          name = default_action;
        }

      dzl_gtk_widget_action (GTK_WIDGET (list_box), group, name, default_target);
    }
}

static void
ide_notifications_button_destroy (GtkWidget *widget)
{
  IdeNotificationsButton *self = (IdeNotificationsButton *)widget;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));

  g_clear_object (&self->filter);
  g_clear_object (&self->model);

  GTK_WIDGET_CLASS (ide_notifications_button_parent_class)->destroy (widget);
}

static void
ide_notifications_button_class_init (IdeNotificationsButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_notifications_button_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-notifications-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, popover);
  gtk_widget_class_bind_template_callback (widget_class, ide_notifications_button_row_activated);
}

static void
ide_notifications_button_init (IdeNotificationsButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (GTK_WIDGET (self),
                                  ide_notifications_button_context_set_cb);
}

/**
 * ide_notifications_button_new:
 *
 * Create a new #IdeNotificationsButton.
 *
 * Returns: (transfer full): a newly created #IdeNotificationsButton
 *
 * Since: 3.32
 */
GtkWidget *
ide_notifications_button_new (void)
{
  return g_object_new (IDE_TYPE_NOTIFICATIONS_BUTTON, NULL);
}
