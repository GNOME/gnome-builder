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

#include <libide-gtk.h>

#include "ide-notifications-button.h"
#include "ide-notification-list-box-row-private.h"
#include "ide-gui-global.h"

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
 */

struct _IdeNotificationsButton
{
  GtkWidget              parent_instance;

  GListModel            *model;
  GtkFilterListModel    *filter;

  /* Template widgets */
  GtkStack              *stack;
  GtkImage              *icon;
  IdeProgressIcon       *progress;
  GtkMenuButton         *menu_button;
  GtkPopover            *popover;
  GtkListBox            *list_box;
  GtkRevealer           *revealer;
};

G_DEFINE_FINAL_TYPE (IdeNotificationsButton, ide_notifications_button, GTK_TYPE_WIDGET)

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
filter_by_has_progress (gpointer item,
                        gpointer user_data)
{
  IdeNotification *notif = item;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (user_data == NULL);

  return ide_notification_get_has_progress (notif);
}

static void
ide_notifications_button_bind_model (IdeNotificationsButton *self,
                                     GListModel             *model)
{
  static GtkCustomFilter *custom;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (custom == NULL)
    custom = gtk_custom_filter_new (filter_by_has_progress, NULL, NULL);

  if (g_set_object (&self->model, model))
    {
      g_clear_object (&self->filter);

      self->filter = gtk_filter_list_model_new (g_object_ref (model),
                                                g_object_ref (GTK_FILTER (custom)));
      gtk_list_box_bind_model (self->list_box,
                               G_LIST_MODEL (self->filter),
                               create_notification_row,
                               self, NULL);
    }
}

static void
ide_notifications_button_notify_has_progress_cb (IdeNotificationsButton *self,
                                                 GParamSpec             *pspec,
                                                 IdeNotifications       *notifications)
{
  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (IDE_IS_NOTIFICATIONS (notifications));

  if (ide_notifications_get_has_progress (notifications))
    {
      gtk_revealer_set_reveal_child (self->revealer, TRUE);
    }
  else
    {
      gtk_menu_button_popdown (self->menu_button);
      gtk_revealer_set_reveal_child (self->revealer, FALSE);
    }
}

static void
ide_notifications_button_notify_progress_is_imprecise_cb (IdeNotificationsButton *self,
                                                          GParamSpec             *pspec,
                                                          IdeNotifications       *notifications)
{
  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (IDE_IS_NOTIFICATIONS (notifications));

  if (ide_notifications_get_progress_is_imprecise (notifications))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->icon));
  else
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->progress));
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

  g_object_bind_property (notifications, "progress",
                          self->progress, "progress",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (notifications,
                           "notify::has-progress",
                           G_CALLBACK (ide_notifications_button_notify_has_progress_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (notifications,
                           "notify::has-progress",
                           G_CALLBACK (ide_notifications_button_notify_progress_is_imprecise_cb),
                           self,
                           G_CONNECT_SWAPPED);

  ide_notifications_button_notify_progress_is_imprecise_cb (self, NULL, notifications);
  ide_notifications_button_notify_has_progress_cb (self, NULL, notifications);
}

static void
ide_notifications_button_row_activated (IdeNotificationsButton    *self,
                                        IdeNotificationListBoxRow *row,
                                        GtkListBox                *list_box)
{
  g_autoptr(GVariant) default_target = NULL;
  g_autofree char *default_action = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));
  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  notif = ide_notification_list_box_row_get_notification (row);

  if (ide_notification_get_default_action (notif, &default_action, &default_target))
    gtk_widget_activate_action_variant (GTK_WIDGET (row), default_action, default_target);
}

static void
ide_notifications_button_dispose (GObject *object)
{
  IdeNotificationsButton *self = (IdeNotificationsButton *)object;

  g_assert (IDE_IS_NOTIFICATIONS_BUTTON (self));

  g_clear_object (&self->filter);
  g_clear_object (&self->model);
  g_clear_pointer ((GtkWidget **)&self->revealer, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_notifications_button_parent_class)->dispose (object);
}

static void
ide_notifications_button_class_init (IdeNotificationsButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_notifications_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-notifications-button.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, icon);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, progress);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationsButton, stack);
  gtk_widget_class_bind_template_callback (widget_class, ide_notifications_button_row_activated);

  g_type_ensure (IDE_TYPE_ANIMATION);
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
 */
GtkWidget *
ide_notifications_button_new (void)
{
  return g_object_new (IDE_TYPE_NOTIFICATIONS_BUTTON, NULL);
}
