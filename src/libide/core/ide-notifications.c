/* ide-notifications.c
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

#define G_LOG_DOMAIN "ide-notifications"

#include "config.h"

#include "ide-macros.h"
#include "ide-notifications.h"

struct _IdeNotifications
{
  IdeObject parent_instance;
};

typedef struct
{
  gdouble progress;
  guint   total;
  guint   imprecise;
} Progress;

typedef struct
{
  const gchar     *id;
  IdeNotification *notif;
} Find;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeNotifications, ide_notifications, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_HAS_PROGRESS,
  PROP_PROGRESS,
  PROP_PROGRESS_IS_IMPRECISE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_notifications_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeNotifications *self = IDE_NOTIFICATIONS (object);

  switch (prop_id)
    {
    case PROP_HAS_PROGRESS:
      g_value_set_boolean (value, ide_notifications_get_has_progress (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_notifications_get_progress (self));
      break;

    case PROP_PROGRESS_IS_IMPRECISE:
      g_value_set_boolean (value, ide_notifications_get_progress_is_imprecise (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notifications_child_notify_progress_cb (IdeNotifications *self,
                                            GParamSpec       *pspec,
                                            IdeNotification  *child)
{
  g_assert (IDE_IS_NOTIFICATIONS (self));
  g_assert (IDE_IS_NOTIFICATION (child));

  ide_object_notify_by_pspec (self, properties [PROP_PROGRESS]);
}

static void
ide_notifications_add (IdeObject         *object,
                       IdeObject         *sibling,
                       IdeObject         *child,
                       IdeObjectLocation  location)
{
  IdeNotifications *self = (IdeNotifications *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_NOTIFICATIONS (object));
  g_assert (IDE_IS_OBJECT (child));

  if (!IDE_IS_NOTIFICATION (child))
    {
      g_warning ("Attempt to add something other than an IdeNotification is not allowed");
      return;
    }

  g_signal_connect_object (child,
                           "notify::progress",
                           G_CALLBACK (ide_notifications_child_notify_progress_cb),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_OBJECT_CLASS (ide_notifications_parent_class)->add (object, sibling, child, location);

  g_list_model_items_changed (G_LIST_MODEL (object), ide_object_get_position (child), 0, 1);
  ide_object_notify_by_pspec (self, properties [PROP_HAS_PROGRESS]);
  ide_object_notify_by_pspec (self, properties [PROP_PROGRESS_IS_IMPRECISE]);
  ide_object_notify_by_pspec (self, properties [PROP_PROGRESS]);
}

static void
ide_notifications_remove (IdeObject *object,
                          IdeObject *child)
{
  IdeNotifications *self = (IdeNotifications *)object;
  guint position;

  g_assert (IDE_IS_NOTIFICATIONS (self));
  g_assert (IDE_IS_OBJECT (child));

  g_signal_handlers_disconnect_by_func (child,
                                        G_CALLBACK (ide_notifications_child_notify_progress_cb),
                                        self);

  position = ide_object_get_position (child);

  IDE_OBJECT_CLASS (ide_notifications_parent_class)->remove (object, child);

  g_list_model_items_changed (G_LIST_MODEL (object), position, 1, 0);
  ide_object_notify_by_pspec (self, properties [PROP_HAS_PROGRESS]);
  ide_object_notify_by_pspec (self, properties [PROP_PROGRESS_IS_IMPRECISE]);
  ide_object_notify_by_pspec (self, properties [PROP_PROGRESS]);
}

static void
ide_notifications_class_init (IdeNotificationsClass *klass)
{
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *object_class = IDE_OBJECT_CLASS (klass);

  g_object_class->get_property = ide_notifications_get_property;

  object_class->add = ide_notifications_add;
  object_class->remove = ide_notifications_remove;

  /**
   * IdeNotifications:has-progress:
   *
   * The "has-progress" property denotes if any of the notifications
   * have progress supported.
   */
  properties [PROP_HAS_PROGRESS] =
    g_param_spec_boolean ("has-progress",
                          "Has Progress",
                          "If any of the notifications have progress",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotifications:progress:
   *
   * The "progress" property is the combination of all of the notifications
   * currently monitored. It is updated when child notifications progress
   * changes.
   */
  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "The combined process of all child notifications",
                         0.0, 1.0, 0.0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotifications:progress-is-imprecise:
   *
   * The "progress-is-imprecise" property indicates that all progress-bearing
   * notifications are imprecise.
   */
  properties [PROP_PROGRESS_IS_IMPRECISE] =
    g_param_spec_boolean ("progress-is-imprecise",
                          "Progress is Imprecise",
                          "If all of the notifications have imprecise progress",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (g_object_class, N_PROPS, properties);
}

static void
ide_notifications_init (IdeNotifications *self)
{
#if 0
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeNotification) notif2 = NULL;
  g_autoptr(IdeNotification) notif3 = NULL;
  g_autoptr(IdeNotification) notif4 = NULL;
  g_autoptr(IdeNotification) notif5 = NULL;
  g_autoptr(IdeNotification) notif6 = NULL;
  g_autoptr(IdeNotification) notif7 = NULL;
  g_autoptr(GIcon) icon1 = NULL;
  g_autoptr(GIcon) icon2 = NULL;
  g_autoptr(GIcon) icon4 = NULL;
  g_autoptr(GIcon) icon5 = NULL;
  g_autoptr(GIcon) icon6 = NULL;
  g_autoptr(GIcon) icon7 = NULL;

  notif = ide_notification_new ();
  ide_notification_set_title (notif, "Builder ready.");
  ide_notification_set_has_progress (notif, FALSE);
  ide_notification_add_button (notif, "Foo", (icon1 = g_icon_new_for_string ("media-playback-pause-symbolic", NULL)), "debugger.pause");
  ide_notifications_add_notification (self, notif);

  notif2 = ide_notification_new ();
  ide_notification_set_title (notif2, "Downloading libdazzle…");
  ide_notification_set_has_progress (notif2, TRUE);
  ide_notification_set_progress (notif2, .75);
  ide_notification_set_default_action (notif2, "win.close");
  ide_notification_add_button (notif2, "Foo", (icon2 = g_icon_new_for_string ("process-stop-symbolic", NULL)), "build-manager.stop");
  ide_notifications_add_notification (self, notif2);

  notif3 = ide_notification_new ();
  ide_notification_set_title (notif3, "SDK Not Installed");
  ide_notification_set_body (notif3, "The org.gnome.Calculator.json build profile requires the org.gnome.Platform runtime. Install it to allow this project to be built.");
  ide_notification_set_has_progress (notif3, FALSE);
  ide_notification_set_progress (notif3, 0);
  ide_notification_add_button (notif3, "Download and Install", NULL, "win.close");
  ide_notification_set_default_action (notif3, "win.close");
  ide_notification_set_urgent (notif3, TRUE);
  ide_notifications_add_notification (self, notif3);

  notif4 = ide_notification_new ();
  ide_notification_set_title (notif4, "Code Analytics Unavailable");
  ide_notification_set_body (notif4, "Code highlighting, error detection, and macros are not fully available, due to this project not being built recently. Rebuild to fully enable these features.");
  ide_notification_set_has_progress (notif4, FALSE);
  ide_notification_set_progress (notif4, 0);
  ide_notification_set_default_action (notif4, "win.close");
  ide_notifications_add_notification (self, notif4);

  notif5 = ide_notification_new ();
  ide_notification_set_title (notif5, "Running Partial Build");
  ide_notification_set_body (notif5, "Diagnostics and autocompletion may be limited until complete.");
  ide_notification_set_has_progress (notif5, TRUE);
  ide_notification_set_progress_is_imprecise (notif5, TRUE);
  ide_notification_set_progress (notif5, 0);
  ide_notification_add_button (notif5, NULL, (icon5 = g_icon_new_for_string ("process-stop-symbolic", NULL)), "win.close");
  ide_notifications_add_notification (self, notif5);

  notif6 = ide_notification_new ();
  ide_notification_set_title (notif6, "Indexing Source Code");
  ide_notification_set_body (notif6, "Search, diagnostics, and autocompletion may be limited until complete.");
  ide_notification_set_has_progress (notif6, TRUE);
  ide_notification_set_progress (notif6, 0);
  ide_notification_set_progress_is_imprecise (notif6, TRUE);
  ide_notification_add_button (notif6, NULL, (icon6 = g_icon_new_for_string ("media-playback-pause-symbolic", NULL)), "win.close");
  ide_notifications_add_notification (self, notif6);

  notif7 = ide_notification_new ();
  ide_notification_set_title (notif7, "Downloading org.gnome.Platform");
  ide_notification_set_body (notif7, "3 minutes remaining");
  ide_notification_set_has_progress (notif7, TRUE);
  ide_notification_set_progress (notif7, 0);
  ide_notification_add_button (notif7, NULL, (icon7 = g_icon_new_for_string ("process-stop-symbolic", NULL)), "win.close");
  ide_notifications_add_notification (self, notif7);

  ide_notification_withdraw_in_seconds (notif, 10);
  ide_notification_withdraw_in_seconds (notif2, 12);
  ide_notification_withdraw_in_seconds (notif3, 14);
  ide_notification_withdraw_in_seconds (notif4, 16);
  ide_notification_withdraw_in_seconds (notif5, 18);
#endif
}

/**
 * ide_notifications_new:
 *
 * Create a new #IdeNotifications.
 *
 * Usually, creating this is not necessary, as the #IdeContext root
 * #IdeObject will create it automatically.
 *
 * Returns: (transfer full): a newly created #IdeNotifications
 */
IdeNotifications *
ide_notifications_new (void)
{
  return g_object_new (IDE_TYPE_NOTIFICATIONS, NULL);
}

/**
 * ide_notifications_add_notification:
 * @self: an #IdeNotifications
 * @notification: an #IdeNotification
 *
 * Adds @notification as a child of @self, sorting it by priority
 * and urgency.
 */
void
ide_notifications_add_notification (IdeNotifications *self,
                                    IdeNotification  *notification)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_NOTIFICATIONS (self));
  g_return_if_fail (IDE_IS_NOTIFICATION (notification));

  ide_object_insert_sorted (IDE_OBJECT (self),
                            IDE_OBJECT (notification),
                            (GCompareDataFunc)ide_notification_compare,
                            NULL);
}

static GType
ide_notifications_get_item_type (GListModel *model)
{
  return IDE_TYPE_NOTIFICATION;
}

static guint
ide_notifications_get_n_items (GListModel *model)
{
  return ide_object_get_n_children (IDE_OBJECT (model));
}

static gpointer
ide_notifications_get_item (GListModel *model,
                            guint       position)
{
  return ide_object_get_nth_child (IDE_OBJECT (model), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_notifications_get_item_type;
  iface->get_n_items = ide_notifications_get_n_items;
  iface->get_item = ide_notifications_get_item;
}

static void
collect_progress_cb (gpointer item,
                     gpointer user_data)
{
  IdeNotification *notif = item;
  Progress *prog = user_data;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (prog != NULL);

  if (ide_notification_get_has_progress (notif))
    {
      if (ide_notification_get_progress_is_imprecise (notif))
        prog->imprecise++;
      else
        prog->progress += ide_notification_get_progress (notif);

      prog->total++;
    }
}

/**
 * ide_notifications_get_progress:
 * @self: a #IdeNotifications
 *
 * Gets the combined progress of the notifications contained in this
 * #IdeNotifications object.
 *
 * Returns: A double between 0.0 and 1.0
 */
gdouble
ide_notifications_get_progress (IdeNotifications *self)
{
  Progress prog = {0};

  g_return_val_if_fail (IDE_IS_NOTIFICATIONS (self), 0.0);

  ide_object_lock (IDE_OBJECT (self));
  ide_object_foreach (IDE_OBJECT (self), collect_progress_cb, &prog);
  ide_object_unlock (IDE_OBJECT (self));

  if (prog.total > 0)
    {
      if (prog.imprecise != prog.total)
        return prog.progress / (gdouble)(prog.total - prog.imprecise);
      else
        return prog.progress / (gdouble)prog.total;
    }

  return 0.0;
}

/**
 * ide_notifications_get_has_progress:
 * @self: a #IdeNotifications
 *
 * Gets if any of the notification support progress updates.
 *
 * Returns: %TRUE if any notification has progress
 */
gboolean
ide_notifications_get_has_progress (IdeNotifications *self)
{
  Progress prog = {0};

  g_return_val_if_fail (IDE_IS_NOTIFICATIONS (self), 0.0);

  ide_object_lock (IDE_OBJECT (self));
  ide_object_foreach (IDE_OBJECT (self), collect_progress_cb, &prog);
  ide_object_unlock (IDE_OBJECT (self));

  return prog.total > 0;
}

/**
 * ide_notifications_get_progress_is_imprecise:
 * @self: a #IdeNotifications
 *
 * Checks if all of the notifications with progress are imprecise.
 *
 * Returns: %TRUE if all progress-supporting notifications are imprecise.
 */
gboolean
ide_notifications_get_progress_is_imprecise (IdeNotifications *self)
{
  Progress prog = {0};

  g_return_val_if_fail (IDE_IS_NOTIFICATIONS (self), 0.0);

  ide_object_lock (IDE_OBJECT (self));
  ide_object_foreach (IDE_OBJECT (self), collect_progress_cb, &prog);
  ide_object_unlock (IDE_OBJECT (self));

  if (prog.total > 0)
    return prog.imprecise == prog.total;

  return FALSE;
}

static void
find_by_id (gpointer item,
            gpointer user_data)
{
  IdeNotification *notif = item;
  Find *find = user_data;
  g_autofree gchar *id = NULL;

  if (find->notif)
    return;

  id = ide_notification_dup_id (notif);

  if (ide_str_equal0 (find->id, id))
    find->notif = g_object_ref (notif);
}

/**
 * ide_notifications_find_by_id:
 * @self: a #IdeNotifications
 * @id: the id of the notification
 *
 * Finds the first #IdeNotification registered with @self with
 * #IdeNotification:id of @id.
 *
 * Returns: (transfer full) (nullable): an #IdeNotification or %NULL
 */
IdeNotification *
ide_notifications_find_by_id (IdeNotifications *self,
                              const gchar      *id)
{
  Find find = { id, NULL };

  g_return_val_if_fail (IDE_IS_NOTIFICATIONS (self), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  ide_object_lock (IDE_OBJECT (self));
  ide_object_foreach (IDE_OBJECT (self), find_by_id, &find);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&find.notif);
}
