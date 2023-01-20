/* ide-transfer.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-transfer"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-debug.h"
#include "ide-macros.h"
#include "ide-marshal.h"
#include "ide-transfer.h"
#include "ide-transfer-manager-private.h"

typedef struct
{
  gchar *icon_name;
  gchar *status;
  gchar *title;
  GCancellable *cancellable;
  gdouble progress;
  gint unique_id;
  guint active : 1;
  guint completed : 1;
} IdeTransferPrivate;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_COMPLETED,
  PROP_ICON_NAME,
  PROP_PROGRESS,
  PROP_STATUS,
  PROP_TITLE,
  N_PROPS
};

enum {
  CANCELLED,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTransfer, ide_transfer, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];
static gint last_unique_id;

static void
ide_transfer_real_execute_async (IdeTransfer         *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_transfer_real_execute_finish (IdeTransfer   *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_TRANSFER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_transfer_finalize (GObject *object)
{
  IdeTransfer *self = (IdeTransfer *)object;
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_clear_pointer (&priv->icon_name, g_free);
  g_clear_pointer (&priv->status, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (ide_transfer_parent_class)->finalize (object);
}

static void
ide_transfer_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeTransfer *self = IDE_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, ide_transfer_get_active (self));
      break;

    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_transfer_get_completed (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, ide_transfer_get_icon_name (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_transfer_get_progress (self));
      break;

    case PROP_STATUS:
      g_value_set_string (value, ide_transfer_get_status (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_transfer_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeTransfer *self = IDE_TRANSFER (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      ide_transfer_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_PROGRESS:
      ide_transfer_set_progress (self, g_value_get_double (value));
      break;

    case PROP_STATUS:
      ide_transfer_set_status (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_transfer_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_class_init (IdeTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_transfer_finalize;
  object_class->get_property = ide_transfer_get_property;
  object_class->set_property = ide_transfer_set_property;

  klass->execute_async = ide_transfer_real_execute_async;
  klass->execute_finish = ide_transfer_real_execute_finish;

  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the transfer is active",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          "Completed",
                          "If the transfer has completed successfully",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon to display next to the transfer",
                         "folder-download-symbolic",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "The progress for the transfer between 0 adn 1",
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STATUS] =
    g_param_spec_string ("status",
                         "Status",
                         "The status message for the transfer",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the transfer",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CANCELLED] =
    g_signal_new ("cancelled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller ( signals [CANCELLED],
                               G_TYPE_FROM_CLASS (klass),
                               ide_marshal_VOID__VOIDv);
}

static void
ide_transfer_init (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  priv->unique_id = ++last_unique_id;
}

static void
ide_transfer_execute_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeTransfer *self = (IdeTransfer *)object;
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER (self));
  g_assert (G_IS_TASK (task));

  priv->active = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);

  ide_transfer_set_progress (self, 1.0);

  if (!IDE_TRANSFER_GET_CLASS (self)->execute_finish (self, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  priv->completed = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMPLETED]);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
ide_transfer_execute_async (IdeTransfer         *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * We already create our own wrapper task so that we can track completion
   * cleanly from the subclass implementation. It also allows us to ensure
   * that the subclasses execute_finish() is guaranteed to be called.
   */
  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_transfer_execute_async);

  /*
   * Wrap our own cancellable so that we can gracefully control
   * the cancellation of the underlying transfer without affecting
   * the callers cancellation state.
   */
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             priv->cancellable,
                             G_CONNECT_SWAPPED);

  priv->active = TRUE;
  priv->completed = FALSE;

  IDE_TRANSFER_GET_CLASS (self)->execute_async (self,
                                                priv->cancellable,
                                                ide_transfer_execute_cb,
                                                g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMPLETED]);

  IDE_EXIT;
}

gboolean
ide_transfer_execute_finish (IdeTransfer   *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TRANSFER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

const gchar *
ide_transfer_get_icon_name (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), NULL);

  return priv->icon_name ?: "folder-download-symbolic";
}

void
ide_transfer_set_icon_name (IdeTransfer *self,
                            const gchar *icon_name)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_if_fail (IDE_IS_TRANSFER (self));

  if (g_set_str (&priv->icon_name, icon_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
    }
}

gdouble
ide_transfer_get_progress (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), 0.0);

  return priv->progress;
}

void
ide_transfer_set_progress (IdeTransfer *self,
                           gdouble      progress)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_if_fail (IDE_IS_TRANSFER (self));

  if (progress != priv->progress)
    {
      priv->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
    }
}

const gchar *
ide_transfer_get_status (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), NULL);

  return priv->status;
}

void
ide_transfer_set_status (IdeTransfer *self,
                         const gchar *status)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_if_fail (IDE_IS_TRANSFER (self));

  if (g_set_str (&priv->status, status))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATUS]);
    }
}

const gchar *
ide_transfer_get_title (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), NULL);

  return priv->title;
}

void
ide_transfer_set_title (IdeTransfer *self,
                        const gchar *title)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_if_fail (IDE_IS_TRANSFER (self));

  if (g_set_str (&priv->title, title))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

void
ide_transfer_cancel (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_if_fail (IDE_IS_TRANSFER (self));

  if (!g_cancellable_is_cancelled (priv->cancellable))
    g_cancellable_cancel (priv->cancellable);

  g_signal_emit (self, signals [CANCELLED], 0);
}

gboolean
ide_transfer_get_completed (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), FALSE);

  return priv->completed;
}

gboolean
ide_transfer_get_active (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), FALSE);

  return priv->active;
}

GQuark
ide_transfer_error_quark (void)
{
  return g_quark_from_static_string ("ide-transfer-error-quark");
}

static void
ide_transfer_notification_notify_completed (IdeTransfer     *self,
                                            GParamSpec      *pspec,
                                            IdeNotification *notif)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TRANSFER (self));
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (ide_transfer_get_completed (self))
    ide_notification_withdraw_in_seconds (notif, -1);
}

/**
 * ide_transfer_create_notification:
 * @self: a #IdeTransfer
 *
 * Creates a new #IdeNotification that is updated with the progress
 * of the #IdeTransfer. This is useful when you need to bridge an
 * #IdeTransfer into something that can be displayed to the user.
 *
 * If the transfer has completed, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): an #IdeNotification or %NULL
 */
IdeNotification *
ide_transfer_create_notification (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);
  g_autoptr(IdeNotification) notif = NULL;
  g_autofree gchar *action_name = NULL;
  g_autoptr(GIcon) icon = NULL;

  g_return_val_if_fail (IDE_IS_TRANSFER (self), NULL);

  if (priv->completed)
    return NULL;

  icon = g_themed_icon_new ("process-stop-symbolic");

  notif = ide_notification_new ();
  ide_notification_set_has_progress (notif, TRUE);
  g_object_bind_property (self, "title", notif, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "status", notif, "body", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "progress", notif, "progress", G_BINDING_SYNC_CREATE);
  g_object_bind_property (self, "icon-name", notif, "icon-name", G_BINDING_SYNC_CREATE);

  /* We avoid using params beacuse it causes buttons to go into toggle-mode
   * using action state.
   */
  action_name = g_strdup_printf ("transfer-manager.cancel-%d", priv->unique_id);
  ide_notification_add_button (notif, _("_Cancel"), icon, action_name);

  g_signal_connect_object (self,
                           "notify::completed",
                           G_CALLBACK (ide_transfer_notification_notify_completed),
                           notif,
                           0);

  g_signal_connect_object (self,
                           "cancelled",
                           G_CALLBACK (ide_notification_withdraw),
                           notif,
                           G_CONNECT_SWAPPED);

  return g_steal_pointer (&notif);
}

gint
_ide_transfer_get_id (IdeTransfer *self)
{
  IdeTransferPrivate *priv = ide_transfer_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TRANSFER (self), 0);

  return priv->unique_id;
}
