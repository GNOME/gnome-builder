/* ide-notification-view.c
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

#define G_LOG_DOMAIN "ide-notification-view"

#include "config.h"

#include "ide-notification-view-private.h"

struct _IdeNotificationView
{
  AdwBin           parent_instance;

  IdeNotification *notification;
  GBindingGroup   *bindings;

  GtkLabel        *label;
  GtkBox          *buttons;
  GtkButton       *default_button;
  GtkImage        *default_button_image;
};

G_DEFINE_FINAL_TYPE (IdeNotificationView, ide_notification_view, ADW_TYPE_BIN)

enum {
  PROP_0,
  PROP_NOTIFICATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_notification_view_notify_icon (IdeNotificationView *self,
                                   GParamSpec          *pspec,
                                   IdeNotification     *notif)
{
  g_autoptr(GIcon) icon = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_NOTIFICATION_VIEW (self));
  g_assert (IDE_IS_NOTIFICATION (notif));

  icon = ide_notification_ref_icon (notif);
  gtk_image_set_pixel_size (self->default_button_image, 16);
  gtk_image_set_from_gicon (self->default_button_image, icon);
  gtk_widget_set_visible (GTK_WIDGET (self->default_button), icon != NULL);
}

static void
connect_notification (IdeNotificationView *self,
                      IdeNotification     *notification)
{
  g_autofree gchar *action_name = NULL;
  g_autoptr(GVariant) target_value = NULL;
  GtkWidget *child;
  guint n_buttons;

  g_assert (IDE_IS_NOTIFICATION_VIEW (self));
  g_assert (!notification || IDE_IS_NOTIFICATION (notification));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->buttons))))
    gtk_box_remove (self->buttons, child);

  if (notification == NULL)
    {
      gtk_widget_hide (GTK_WIDGET (self->label));
      gtk_widget_hide (GTK_WIDGET (self->default_button));
      gtk_widget_hide (GTK_WIDGET (self->buttons));
      return;
    }

  g_signal_connect_object (notification,
                           "notify::icon",
                           G_CALLBACK (ide_notification_view_notify_icon),
                           self,
                           G_CONNECT_SWAPPED);
  ide_notification_view_notify_icon (self, NULL, notification);

  /*
   * Setup the default action button (which is shown right after the label
   * containing notification title).
   */

  if (ide_notification_get_default_action (notification, &action_name, &target_value))
    {
      gtk_actionable_set_action_name (GTK_ACTIONABLE (self->default_button), action_name);
      gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->default_button), target_value);
    }

  /*
   * Now add all of the buttons requested by the notification.
   */

  ide_object_lock (IDE_OBJECT (notification));

  n_buttons = ide_notification_get_n_buttons (notification);

  for (guint i = 0; i < n_buttons; i++)
    {
      g_autofree gchar *action = NULL;
      g_autofree gchar *label = NULL;
      g_autoptr(GIcon) button_icon = NULL;
      g_autoptr(GVariant) target = NULL;

      if (ide_notification_get_button (notification, i, &label, &button_icon, &action, &target) &&
          button_icon != NULL &&
          action_name != NULL)
        {
          GtkButton *button;

          button = g_object_new (GTK_TYPE_BUTTON,
                                 "child", g_object_new (GTK_TYPE_IMAGE,
                                                        "gicon", button_icon,
                                                        NULL),
                                 "action-name", action,
                                 "action-target", target,
                                 "has-tooltip", TRUE,
                                 "tooltip-text", label,
                                 NULL);
          gtk_box_append (self->buttons, GTK_WIDGET (button));
        }
    }

  ide_object_unlock (IDE_OBJECT (notification));
}

static void
ide_notification_view_finalize (GObject *object)
{
  IdeNotificationView *self = (IdeNotificationView *)object;

  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_object (&self->bindings);
  g_clear_object (&self->notification);

  G_OBJECT_CLASS (ide_notification_view_parent_class)->finalize (object);
}

static void
ide_notification_view_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeNotificationView *self = IDE_NOTIFICATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      g_value_set_object (value, ide_notification_view_get_notification (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_view_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeNotificationView *self = IDE_NOTIFICATION_VIEW (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      ide_notification_view_set_notification (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_view_class_init (IdeNotificationViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_notification_view_finalize;
  object_class->get_property = ide_notification_view_get_property;
  object_class->set_property = ide_notification_view_set_property;

  /**
   * IdeNotificationView:notification:
   *
   * The "notification" property is the #IdeNotification to be displayed.
   */
  properties [PROP_NOTIFICATION] =
    g_param_spec_object ("notification",
                         "Notification",
                         "The IdeNotification to be viewed",
                         IDE_TYPE_NOTIFICATION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-notification-view.ui");
  gtk_widget_class_set_css_name (widget_class, "notification");
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationView, label);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationView, buttons);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationView, default_button);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationView, default_button_image);
}

static void
ide_notification_view_init (IdeNotificationView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bindings = g_binding_group_new ();

  g_binding_group_bind (self->bindings, "title", self->label, "label", G_BINDING_SYNC_CREATE);
}

/**
 * ide_notification_view_new:
 *
 * Create a new #IdeNotificationView to visualize a notification within
 * the #IdeOmniBar.
 *
 * Returns: (transfer full): a newly created #IdeNotificationView
 */
GtkWidget *
ide_notification_view_new (void)
{
  return g_object_new (IDE_TYPE_NOTIFICATION_VIEW, NULL);
}

/**
 * ide_notification_view_get_notification:
 * @self: an #IdeNotificationView
 *
 * Gets the #IdeNotification that is being viewed.
 *
 * Returns: (transfer none) (nullable): an #IdeNotification or %NULL
 */
IdeNotification *
ide_notification_view_get_notification (IdeNotificationView *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_NOTIFICATION_VIEW (self), NULL);

  return self->notification;
}

void
ide_notification_view_set_notification (IdeNotificationView *self,
                                        IdeNotification     *notification)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_NOTIFICATION_VIEW (self));
  g_return_if_fail (!notification || IDE_IS_NOTIFICATION (notification));

  if (g_set_object (&self->notification, notification))
    {
      g_binding_group_set_source (self->bindings, notification);
      connect_notification (self, notification);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NOTIFICATION]);
    }
}
