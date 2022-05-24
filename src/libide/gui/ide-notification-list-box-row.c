/* ide-notification-list-box-row.c
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

#define G_LOG_DOMAIN "ide-notification-list-box-row"

#include "config.h"

#include <libide-gtk.h>

#include "ide-notification-list-box-row-private.h"

struct _IdeNotificationListBoxRow
{
  GtkListBoxRow    parent_instance;

  IdeNotification *notification;

  GtkLabel        *body;
  GtkLabel        *title;
  GtkBox          *lower_button_area;
  GtkBox          *side_button_area;
  GtkBox          *buttons;
  GtkProgressBar  *progress;

  guint            compact : 1;
};

G_DEFINE_FINAL_TYPE (IdeNotificationListBoxRow, ide_notification_list_box_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_COMPACT,
  PROP_NOTIFICATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
setup_buttons_locked (IdeNotificationListBoxRow *self)
{
  g_autofree gchar *body = NULL;
  g_autofree gchar *title = NULL;
  guint n_buttons;

  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (self));
  g_assert (self->notification != NULL);

  title = ide_notification_dup_title (self->notification);
  body = ide_notification_dup_body (self->notification);

  n_buttons = ide_notification_get_n_buttons (self->notification);

  for (guint i = 0; i < n_buttons; i++)
    {
      g_autofree gchar *action = NULL;
      g_autofree gchar *label = NULL;
      g_autoptr(GIcon) icon = NULL;
      g_autoptr(GVariant) target = NULL;

      if (ide_notification_get_button (self->notification, i, &label, &icon, &action, &target))
        {
          GtkButton *button;
          GtkWidget *child = NULL;

          if (action == NULL || (label == NULL && icon == NULL))
            continue;

          if (label != NULL && (!self->compact || icon == NULL))
            child = g_object_new (GTK_TYPE_LABEL,
                                  "label", label,
                                  "use-underline", TRUE,
                                  NULL);
          else if (icon != NULL)
            child = g_object_new (GTK_TYPE_IMAGE,
                                  "pixel-size", 16,
                                  "gicon", icon,
                                  NULL);

          g_assert (GTK_IS_WIDGET (child));

          button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                                 "child", child,
                                 "action-name", action,
                                 "action-target", target,
                                 NULL);


          if (!self->compact)
            {
              g_object_set (button, "width-request", 100, NULL);
              gtk_widget_add_css_class (GTK_WIDGET (button), "suggested-action");
            }
          else
            gtk_widget_add_css_class (GTK_WIDGET (button), "circular");

          g_assert (GTK_IS_WIDGET (button));

          gtk_box_append (self->buttons, GTK_WIDGET (button));
        }
    }

  /* Always show labels when compact+buttons for alignment. */
  gtk_widget_set_visible (GTK_WIDGET (self->body),
                          !ide_str_empty0 (body) || (self->compact && n_buttons > 0));
  gtk_widget_set_visible (GTK_WIDGET (self->title),
                          !ide_str_empty0 (title) || (self->compact && n_buttons > 0));

  gtk_widget_set_visible (GTK_WIDGET (self->buttons), n_buttons > 0);
}

/**
 * ide_notification_list_box_row_new:
 *
 * Create a new #IdeNotificationListBoxRow.
 *
 * Returns: (transfer full): a newly created #IdeNotificationListBoxRow
 */
GtkWidget *
ide_notification_list_box_row_new (IdeNotification *notification)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION (notification), NULL);

  return g_object_new (IDE_TYPE_NOTIFICATION_LIST_BOX_ROW,
                       "notification", notification,
                       NULL);
}

static void
ide_notification_list_box_row_constructed (GObject *object)
{
  IdeNotificationListBoxRow *self = (IdeNotificationListBoxRow *)object;
  g_autofree gchar *body = NULL;
  g_autofree gchar *title = NULL;

  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (self));

  if (self->notification == NULL)
    {
      g_warning ("%s created without an IdeNotification!",
                 G_OBJECT_TYPE_NAME (self));
      goto chain_up;
    }

  ide_object_lock (IDE_OBJECT (self->notification));

  body = ide_notification_dup_body (self->notification);
  title = ide_notification_dup_title (self->notification);

  g_object_bind_property (self->notification, "title", self->title, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->notification, "body", self->body, "label", G_BINDING_SYNC_CREATE);

  /* Always show labels when compact+buttons for alignment. */
  gtk_widget_set_visible (GTK_WIDGET (self->body),
                          !ide_str_empty0 (body) ||
                          (self->compact && ide_notification_get_n_buttons (self->notification)));
  gtk_widget_set_visible (GTK_WIDGET (self->title),
                          !ide_str_empty0 (title) ||
                          (self->compact && ide_notification_get_n_buttons (self->notification)));

  if (ide_notification_get_urgent (self->notification))
    gtk_widget_add_css_class (GTK_WIDGET (self), "needs-attention");

  gtk_widget_set_visible (GTK_WIDGET (self->progress),
                          ide_notification_get_has_progress (self->notification));
  g_object_bind_property (self->notification, "progress",
                          self->progress, "fraction",
                          G_BINDING_SYNC_CREATE);

  setup_buttons_locked (self);

  if (ide_notification_get_progress_is_imprecise (self->notification))
    ide_gtk_progress_bar_start_pulsing (self->progress);

  ide_object_unlock (IDE_OBJECT (self->notification));

chain_up:
  G_OBJECT_CLASS (ide_notification_list_box_row_parent_class)->constructed (object);
}

static void
ide_notification_list_box_row_dispose (GObject *object)
{
  IdeNotificationListBoxRow *self = (IdeNotificationListBoxRow *)object;

  if (self->progress != NULL)
    ide_gtk_progress_bar_stop_pulsing (self->progress);

  g_clear_object (&self->notification);

  G_OBJECT_CLASS (ide_notification_list_box_row_parent_class)->dispose (object);
}

static void
ide_notification_list_box_row_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  IdeNotificationListBoxRow *self = IDE_NOTIFICATION_LIST_BOX_ROW (object);

  switch (prop_id)
    {
    case PROP_COMPACT:
      g_value_set_boolean (value, ide_notification_list_box_row_get_compact (self));
      break;

    case PROP_NOTIFICATION:
      g_value_set_object (value, self->notification);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_list_box_row_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  IdeNotificationListBoxRow *self = IDE_NOTIFICATION_LIST_BOX_ROW (object);

  switch (prop_id)
    {
    case PROP_COMPACT:
      ide_notification_list_box_row_set_compact (self, g_value_get_boolean (value));
      break;

    case PROP_NOTIFICATION:
      self->notification = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_list_box_row_class_init (IdeNotificationListBoxRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_notification_list_box_row_constructed;
  object_class->dispose = ide_notification_list_box_row_dispose;
  object_class->get_property = ide_notification_list_box_row_get_property;
  object_class->set_property = ide_notification_list_box_row_set_property;

  properties [PROP_COMPACT] =
    g_param_spec_boolean ("compact",
                          "Compact",
                          "If the compact button mode should be used",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NOTIFICATION] =
    g_param_spec_object ("notification",
                         "Notification",
                         "The notification to display",
                         IDE_TYPE_NOTIFICATION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-notification-list-box-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, body);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, buttons);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, lower_button_area);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, progress);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, side_button_area);
  gtk_widget_class_bind_template_child (widget_class, IdeNotificationListBoxRow, title);
}

static void
ide_notification_list_box_row_init (IdeNotificationListBoxRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * ide_notification_list_box_row_get_notification:
 * @self: a #IdeNotificationListBoxRow
 *
 * Returns: (transfer none) (nullable): an #IdeNotification
 */
IdeNotification *
ide_notification_list_box_row_get_notification (IdeNotificationListBoxRow *self)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION_LIST_BOX_ROW (self), NULL);

  return self->notification;
}

gboolean
ide_notification_list_box_row_get_compact (IdeNotificationListBoxRow *self)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION_LIST_BOX_ROW (self), FALSE);

  return self->compact;
}

void
ide_notification_list_box_row_set_compact (IdeNotificationListBoxRow *self,
                                           gboolean                   compact)
{
  GtkWidget *child;
  GtkBox *parent;

  g_return_if_fail (IDE_IS_NOTIFICATION_LIST_BOX_ROW (self));

  if (self->compact != compact)
    {
      self->compact = compact;

      g_object_ref (self->buttons);

      while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->buttons))))
        gtk_box_remove (self->buttons, child);

      parent = GTK_BOX (gtk_widget_get_parent (GTK_WIDGET (self->buttons)));
      gtk_box_remove (parent, GTK_WIDGET (self->buttons));
      gtk_widget_hide (GTK_WIDGET (parent));

      if (compact)
        parent = self->side_button_area;
      else
        parent = self->lower_button_area;

      gtk_box_append (parent, GTK_WIDGET (self->buttons));

      g_object_unref (self->buttons);

      gtk_label_set_width_chars (self->title, self->compact ? 40 : 55);
      gtk_label_set_max_width_chars (self->title, self->compact ? 40 : 55);

      gtk_label_set_width_chars (self->body, self->compact ? 40 : 55);
      gtk_label_set_max_width_chars (self->body, self->compact ? 40 : 55);

      if (self->notification != NULL)
        {
          ide_object_lock (IDE_OBJECT (self->notification));
          setup_buttons_locked (self);
          gtk_widget_set_visible (GTK_WIDGET (parent),
                                  ide_notification_get_n_buttons (self->notification) > 0);
          ide_object_unlock (IDE_OBJECT (self->notification));
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMPACT]);
    }
}
