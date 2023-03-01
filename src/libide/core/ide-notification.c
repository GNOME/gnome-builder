/* ide-notification.c
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

#define G_LOG_DOMAIN "ide-notification"

#include "config.h"

#include "ide-macros.h"
#include "ide-notification.h"
#include "ide-notifications.h"

typedef struct
{
  gchar    *id;
  gchar    *title;
  gchar    *body;
  GIcon    *icon;
  gchar    *default_action;
  GVariant *default_target;
  GArray   *buttons;
  gdouble   progress;
  gint      priority;
  guint     has_progress : 1;
  guint     progress_is_imprecise : 1;
  guint     urgent : 1;
} IdeNotificationPrivate;

typedef struct
{
  gchar    *label;
  GIcon    *icon;
  gchar    *action;
  GVariant *target;
} Button;

G_DEFINE_TYPE_WITH_PRIVATE (IdeNotification, ide_notification, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BODY,
  PROP_HAS_PROGRESS,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_PRIORITY,
  PROP_PROGRESS,
  PROP_PROGRESS_IS_IMPRECISE,
  PROP_TITLE,
  PROP_URGENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
clear_button (Button *button)
{
  g_clear_pointer (&button->label, g_free);
  g_clear_pointer (&button->action, g_free);
  g_clear_pointer (&button->target, g_variant_unref);
  g_clear_object (&button->icon);
}

static gchar *
ide_notification_repr (IdeObject *object)
{
  IdeNotification *self = IDE_NOTIFICATION (object);
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  return g_strdup_printf ("%s label=%s",
                          G_OBJECT_TYPE_NAME (self),
                          priv->title);
}

static void
ide_notification_destroy (IdeObject *object)
{
  IdeNotification *self = (IdeNotification *)object;
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->body, g_free);
  g_clear_pointer (&priv->default_action, g_free);
  g_clear_pointer (&priv->default_target, g_variant_unref);
  g_clear_pointer (&priv->buttons, g_array_unref);
  g_clear_object (&priv->icon);

  IDE_OBJECT_CLASS (ide_notification_parent_class)->destroy (object);
}

static void
ide_notification_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeNotification *self = IDE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_BODY:
      g_value_take_string (value, ide_notification_dup_body (self));
      break;

    case PROP_HAS_PROGRESS:
      g_value_set_boolean (value, ide_notification_get_has_progress (self));
      break;

    case PROP_ICON:
      g_value_take_object (value, ide_notification_ref_icon (self));
      break;

    case PROP_ID:
      g_value_take_string (value, ide_notification_dup_id (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, ide_notification_get_priority (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_notification_get_progress (self));
      break;

    case PROP_PROGRESS_IS_IMPRECISE:
      g_value_set_boolean (value, ide_notification_get_progress_is_imprecise (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, ide_notification_dup_title (self));
      break;

    case PROP_URGENT:
      g_value_set_boolean (value, ide_notification_get_urgent (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeNotification *self = IDE_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_BODY:
      ide_notification_set_body (self, g_value_get_string (value));
      break;

    case PROP_HAS_PROGRESS:
      ide_notification_set_has_progress (self, g_value_get_boolean (value));
      break;

    case PROP_ICON:
      ide_notification_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      ide_notification_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ID:
      ide_notification_set_id (self, g_value_get_string (value));
      break;

    case PROP_PRIORITY:
      ide_notification_set_priority (self, g_value_get_int (value));
      break;

    case PROP_PROGRESS:
      ide_notification_set_progress (self, g_value_get_double (value));
      break;

    case PROP_PROGRESS_IS_IMPRECISE:
      ide_notification_set_progress_is_imprecise (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      ide_notification_set_title (self, g_value_get_string (value));
      break;

    case PROP_URGENT:
      ide_notification_set_urgent (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_class_init (IdeNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *ide_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_notification_get_property;
  object_class->set_property = ide_notification_set_property;

  ide_object_class->destroy = ide_notification_destroy;
  ide_object_class->repr = ide_notification_repr;

  /**
   * IdeNotification:body:
   *
   * The "body" property is the main body of text for the notification.
   * Not all notifications need this, but more complex notifications might.
   */
  properties [PROP_BODY] =
    g_param_spec_string ("body",
                         "Body",
                         "The body of the notification",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:has-progress:
   *
   * The "has-progress" property denotes the notification will receive
   * updates to the #IdeNotification:progress property.
   */
  properties [PROP_HAS_PROGRESS] =
    g_param_spec_boolean ("has-progress",
                          "Has Progress",
                          "If the notification supports progress updates",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:icon:
   *
   * The "icon" property is an optional icon that may be shown next to
   * the notification title and body under certain senarios.
   */
  properties [PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "The icon for the notification, if any",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:icon-name:
   *
   * The "icon-name" property is a helper to make setting #IdeNotification:icon
   * more convenient.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "An icon-name to use to set IdeNotification:icon",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:id:
   *
   * The "id" property is an optional identifier that can be used to locate
   * the notification later.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "An optional identifier for the notification",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:priority:
   *
   * The "priority" property is used to sort the notification in order of
   * importance when displaying to the user.
   *
   * You may also use the #IdeNotification:urgent property to raise the
   * importance of a message to the user.
   */
  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the notification",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:progress:
   *
   * The "progress" property is a value between 0.0 and 1.0 describing the progress of
   * the operation for which the notification represents.
   *
   * This property is ignored if #IdeNotification:has-progress is unset.
   */
  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "The progress for the notification, if any",
                         0.0, 1.0, 0.0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:progress-is-imprecise:
   *
   * The "progress-is-imprecise" property indicates that the notification has
   * progress, but it is imprecise.
   *
   * The UI may show a bouncing progress bar if set.
   */
  properties [PROP_PROGRESS_IS_IMPRECISE] =
    g_param_spec_boolean ("progress-is-imprecise",
                          "Progress is Imprecise",
                          "If the notification supports progress, but is imprecise",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:title:
   *
   * The "title" property is the main text to show the user. It may be
   * displayed more prominently such as in the titlebar.
   */
  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the notification",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeNotification:urgent:
   *
   * If the notification is urgent. These notifications will be displayed with
   * higher priority than those without the urgent property set.
   */
  properties [PROP_URGENT] =
    g_param_spec_boolean ("urgent",
                          "Urgent",
                          "If it is urgent the user see the notification",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_notification_init (IdeNotification *self)
{
}

/**
 * ide_notification_new:
 *
 * Creates a new #IdeNotification.
 *
 * To "send" the notification, you should attach it to the #IdeNotifications
 * object which can be found under the root #IdeObject. To simplify this,
 * the ide_notification_attach() function is provided to locate the
 * #IdeNotifications object using any #IdeObject you have access to.
 *
 * ```
 * IdeNotification *notif = ide_notification_new ();
 * setup_notification (notify);
 * ide_notification_attach (notif, IDE_OBJECT (some_object));
 * ```
 */
IdeNotification *
ide_notification_new (void)
{
  return g_object_new (IDE_TYPE_NOTIFICATION, NULL);
}

/**
 * ide_notification_attach:
 * @self: an #IdeNotifications
 * @object: an #IdeObject
 *
 * This function will locate the #IdeNotifications object starting from
 * @object and attach @self as a child to that object.
 */
void
ide_notification_attach (IdeNotification *self,
                         IdeObject       *object)
{
  g_autoptr(IdeObject) root = NULL;
  g_autoptr(IdeObject) child = NULL;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (IDE_IS_OBJECT (object));

  root = ide_object_ref_root (object);
  child = ide_object_get_child_typed (root, IDE_TYPE_NOTIFICATIONS);

  if (child != NULL)
    ide_notifications_add_notification (IDE_NOTIFICATIONS (child), self);
  else
    g_warning ("Failed to locate IdeNotifications from %s", G_OBJECT_TYPE_NAME (object));
}

/**
 * ide_notification_dup_id:
 *
 * Copies the id of the notification and returns it to the caller after locking
 * the object. A copy is used to avoid thread-races.
 */
gchar *
ide_notification_dup_id (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gchar *ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_strdup (priv->id);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_notification_set_id:
 * @self: an #IdeNotification
 * @id: (nullable): a string containing the id, or %NULL
 *
 * Sets the #IdeNotification:id property.
 */
void
ide_notification_set_id (IdeNotification *self,
                         const gchar     *id)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_str (&priv->id, id))
    {
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_ID]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_notification_dup_title:
 *
 * Copies the current title and returns it to the caller after locking the
 * object. A copy is used to avoid thread-races.
 */
gchar *
ide_notification_dup_title (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gchar *ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_strdup (priv->title);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_notification_set_title:
 * @self: an #IdeNotification
 * @title: (nullable): a string containing the title text, or %NULL
 *
 * Sets the #IdeNotification:title property.
 */
void
ide_notification_set_title (IdeNotification *self,
                            const gchar     *title)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_str (&priv->title, title))
    {
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_TITLE]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_notification_dup_body:
 *
 * Copies the current body and returns it to the caller after locking the
 * object. A copy is used to avoid thread-races.
 */
gchar *
ide_notification_dup_body (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gchar *ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_strdup (priv->body);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_notification_set_body:
 * @self: an #IdeNotification
 * @body: (nullable): a string containing the body text, or %NULL
 *
 * Sets the #IdeNotification:body property.
 */
void
ide_notification_set_body (IdeNotification *self,
                           const gchar     *body)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_str (&priv->body, body))
    {
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_BODY]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_notification_ref_icon:
 *
 * Gets the icon for the notification, and returns a new reference
 * to the #GIcon.
 *
 * Returns: (transfer full) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_notification_ref_icon (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  GIcon *ret = NULL;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  g_set_object (&ret, priv->icon);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

void
ide_notification_set_icon (IdeNotification *self,
                           GIcon           *icon)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_object (&priv->icon, icon))
    ide_object_notify_by_pspec (self, properties [PROP_ICON]);
  ide_object_unlock (IDE_OBJECT (self));
}

void
ide_notification_set_icon_name (IdeNotification *self,
                                const gchar     *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);
  ide_notification_set_icon (self, icon);
}

gint
ide_notification_get_priority (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gint ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), 0);

  ide_object_lock (IDE_OBJECT (self));
  ret = priv->priority;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

void
ide_notification_set_priority (IdeNotification *self,
                               gint             priority)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  ide_object_lock (IDE_OBJECT (self));
  if (priv->priority != priority)
    {
      priv->priority = priority;
      ide_object_notify_by_pspec (self, properties [PROP_PRIORITY]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

gboolean
ide_notification_get_urgent (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gboolean ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  ret = priv->urgent;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

void
ide_notification_set_urgent (IdeNotification *self,
                             gboolean         urgent)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  urgent = !!urgent;

  ide_object_lock (IDE_OBJECT (self));
  if (priv->urgent != urgent)
    {
      priv->urgent = urgent;
      ide_object_notify_by_pspec (self, properties [PROP_URGENT]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

guint
ide_notification_get_n_buttons (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  guint ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  if (priv->buttons != NULL)
    ret = priv->buttons->len;
  else
    ret = 0;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

/**
 * ide_notification_get_button:
 * @self: an #IdeNotification
 * @label: (out) (optional): a location for the button label
 * @icon: (out) (optional): a location for the button icon
 * @action: (out) (optional): a location for the button action name
 * @target: (out) (optional): a location for the button action target
 *
 * Gets the button indexed by @button, and stores information about the
 * button into the various out parameters @label, @icon, @action, and @target.
 *
 * Caller should check for the number of buttons using
 * ide_notification_get_n_buttons() to determine the numerical range of
 * indexes to provide for @button.
 *
 * To avoid racing with threads modifying notifications, the caller can
 * hold a recursive lock across the function calls using ide_object_lock()
 * and ide_object_unlock().
 *
 * Returns: %TRUE if @button was found; otherwise %FALSE
 */
gboolean
ide_notification_get_button (IdeNotification  *self,
                             guint             button,
                             gchar           **label,
                             GIcon           **icon,
                             gchar           **action,
                             GVariant        **target)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  if (priv->buttons != NULL)
    {
      if (button < priv->buttons->len)
        {
          Button *b = &g_array_index (priv->buttons, Button, button);

          if (label)
            *label = g_strdup (b->label);
          if (icon)
            g_set_object (icon, b->icon);
          if (action)
            *action = g_strdup (b->action);
          if (target)
            *target = b->target ? g_variant_ref (b->target) : NULL;
          ret = TRUE;
        }
    }
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

/**
 * ide_notification_add_button:
 * @self: an #IdeNotification
 * @label: the label for the button
 * @icon: (nullable): an optional icon for the button
 * @detailed_action: a detailed action name (See #GAction)
 *
 * Adds a new button that may be displayed with the notification.
 *
 * See also: ide_notification_add_button_with_target_value().
 */
void
ide_notification_add_button (IdeNotification *self,
                             const gchar     *label,
                             GIcon           *icon,
                             const gchar     *detailed_action)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) target_value = NULL;
  g_autofree gchar *action_name = NULL;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (label || icon);
  g_return_if_fail (!icon || G_IS_ICON (icon));
  g_return_if_fail (detailed_action != NULL);

  if (!g_action_parse_detailed_name (detailed_action, &action_name, &target_value, &error))
    g_warning ("Failed to parse detailed_action: %s", error->message);
  else
    ide_notification_add_button_with_target_value (self, label, icon, action_name, target_value);
}

/**
 * ide_notification_add_button_with_target_value:
 * @self: an #IdeNotification
 * @label: the label for the button
 * @icon: (nullable): an optional icon for the button
 * @action: an action name (See #GAction)
 * @target: (nullable): an optional #GVariant for the action target
 *
 * Adds a new button, used the parsed #GVariant format for the action
 * target.
 */
void
ide_notification_add_button_with_target_value (IdeNotification *self,
                                               const gchar     *label,
                                               GIcon           *icon,
                                               const gchar     *action,
                                               GVariant        *target)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  Button b = {0};

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (label || icon);
  g_return_if_fail (action != NULL);

  b.label = g_strdup (label);
  g_set_object (&b.icon, icon);
  b.action = g_strdup (action);
  b.target = target ? g_variant_ref (target) : NULL;

  ide_object_lock (IDE_OBJECT (self));
  if (priv->buttons == NULL)
    {
      priv->buttons = g_array_new (FALSE, FALSE, sizeof b);
      g_array_set_clear_func (priv->buttons, (GDestroyNotify)clear_button);
    }
  g_array_append_val (priv->buttons, b);
  ide_object_unlock (IDE_OBJECT (self));
}

gboolean
ide_notification_get_default_action (IdeNotification  *self,
                                     gchar           **action,
                                     GVariant        **target)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), FALSE);


  ide_object_lock (IDE_OBJECT (self));
  if (priv->default_action != NULL)
    {
      if (action)
        *action = g_strdup (priv->default_action);
      if (target)
        *target = priv->default_target ? g_variant_ref (priv->default_target) : NULL;
      ret = TRUE;
    }
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

void
ide_notification_set_default_action (IdeNotification *self,
                                     const gchar     *detailed_action)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) target_value = NULL;
  g_autofree gchar *action_name = NULL;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (detailed_action != NULL);

  if (!g_action_parse_detailed_name (detailed_action, &action_name, &target_value, &error))
    g_warning ("Failed to parse detailed_action: %s", error->message);
  else
    ide_notification_set_default_action_and_target_value (self, action_name, target_value);
}

void
ide_notification_set_default_action_and_target_value (IdeNotification *self,
                                                      const gchar     *action,
                                                      GVariant        *target)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));
  g_return_if_fail (action != NULL);

  ide_object_lock (IDE_OBJECT (self));

  g_set_str (&priv->default_action, action);

  if (priv->default_target != NULL &&
      target != NULL &&
      g_variant_equal (priv->default_target, target))
    goto unlock;

  g_clear_pointer (&priv->default_target, g_variant_unref);
  priv->default_target = target ? g_variant_ref (target) : NULL;

unlock:
  ide_object_unlock (IDE_OBJECT (self));
}

gint
ide_notification_compare (IdeNotification *a,
                          IdeNotification *b)
{
  IdeNotificationPrivate *a_priv = ide_notification_get_instance_private (a);
  IdeNotificationPrivate *b_priv = ide_notification_get_instance_private (b);

  if (a_priv->urgent)
    {
      if (!b_priv->urgent)
        return -1;
    }

  if (b_priv->urgent)
    {
      if (!a_priv->urgent)
        return 1;
    }

  return a_priv->priority - b_priv->priority;
}

/**
 * ide_notification_get_progress:
 * @self: a #IdeNotification
 *
 * Gets the progress for the notification.
 *
 * Returns: a value between 0.0 and 1.0
 */
gdouble
ide_notification_get_progress (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gdouble ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), 0.0);

  ide_object_lock (IDE_OBJECT (self));
  ret = priv->progress;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

/**
 * ide_notification_set_progress:
 * @self: a #IdeNotification
 * @progress: a value between 0.0 and 1.0
 *
 * Sets the progress for the notification.
 */
void
ide_notification_set_progress (IdeNotification *self,
                               gdouble          progress)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  progress = CLAMP (progress, 0.0, 1.0);

  ide_object_lock (IDE_OBJECT (self));
  if (priv->progress != progress)
    {
      priv->progress = progress;
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_PROGRESS]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_notification_get_has_progress:
 * @self: a #IdeNotification
 *
 * Gets if the notification supports progress updates.
 *
 * Returns: %TRUE if progress updates are supported.
 */
gboolean
ide_notification_get_has_progress (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gboolean ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), 0.0);

  ide_object_lock (IDE_OBJECT (self));
  ret = priv->has_progress;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

/**
 * ide_notification_set_has_progress:
 * @self: a #IdeNotification
 * @has_progress: if @notification supports progress
 *
 * Set to %TRUE if the notification supports progress updates.
 */
void
ide_notification_set_has_progress (IdeNotification *self,
                                   gboolean         has_progress)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  has_progress = !!has_progress;

  ide_object_lock (IDE_OBJECT (self));
  if (priv->has_progress != has_progress)
    {
      priv->has_progress = has_progress;
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_HAS_PROGRESS]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

gboolean
ide_notification_get_progress_is_imprecise (IdeNotification *self)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);
  gboolean ret;

  g_return_val_if_fail (IDE_IS_NOTIFICATION (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  ret = priv->progress_is_imprecise;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

void
ide_notification_set_progress_is_imprecise (IdeNotification *self,
                                            gboolean         progress_is_imprecise)
{
  IdeNotificationPrivate *priv = ide_notification_get_instance_private (self);

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  progress_is_imprecise = !!progress_is_imprecise;

  ide_object_lock (IDE_OBJECT (self));
  if (priv->progress_is_imprecise != progress_is_imprecise)
    {
      priv->progress_is_imprecise = progress_is_imprecise;
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_PROGRESS_IS_IMPRECISE]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_notification_withdraw:
 * @self: a #IdeNotification
 *
 * Withdraws the notification by removing it from the #IdeObject parent it
 * belongs to.
 */
void
ide_notification_withdraw (IdeNotification *self)
{
  g_autoptr(IdeObject) parent = NULL;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  g_object_ref (self);
  ide_object_lock (IDE_OBJECT (self));

  if ((parent = ide_object_ref_parent (IDE_OBJECT (self))))
    ide_object_remove (parent, IDE_OBJECT (self));

  ide_object_unlock (IDE_OBJECT (self));
  g_object_unref (self);
}

static gboolean
do_withdrawal (gpointer data)
{
  ide_notification_withdraw (data);
  return FALSE;
}

/**
 * ide_notification_withdraw_in_seconds:
 * @self: a #IdeNotification
 * @seconds: number of seconds to withdraw after, or less than zero for a
 *   sensible default.
 *
 * Withdraws @self from it's #IdeObject parent after @seconds have passed.
 */
void
ide_notification_withdraw_in_seconds (IdeNotification *self,
                                      gint             seconds)
{
  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  if (seconds < 0)
    seconds = 15;

  g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                              seconds,
                              do_withdrawal,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * ide_notification_file_progress_callback:
 *
 * This function is a #GFileProgressCallback helper that will update the
 * #IdeNotification:fraction property. @user_data must be an #IdeNotification.
 *
 * Remember to make sure to unref the #IdeNotification instance with
 * g_object_unref() during the #GDestroyNotify.
 */
void
ide_notification_file_progress_callback (goffset  current_num_bytes,
                                         goffset  total_num_bytes,
                                         gpointer user_data)
{
  IdeNotification *self = user_data;
  double fraction;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  if (total_num_bytes)
    fraction = (double)current_num_bytes / (double)total_num_bytes;
  else
    fraction = .0;

  if (fraction < .0)
    fraction = .0;
  else if (fraction > 1.)
    fraction = 1.;

  ide_notification_set_progress (self, fraction);
}

void
ide_notification_flatpak_progress_callback (const char *status,
                                            guint       notification,
                                            gboolean    estimating,
                                            gpointer    user_data)
{
  IdeNotification *self = user_data;

  g_return_if_fail (IDE_IS_NOTIFICATION (self));

  ide_notification_set_body (self, status);
  ide_notification_set_progress (self, (gdouble)notification / 100.0);
}
