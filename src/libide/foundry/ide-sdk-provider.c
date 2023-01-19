/* ide-sdk-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-sdk-provider"

#include "config.h"

#include "ide-marshal.h"

#include "ide-sdk-private.h"
#include "ide-sdk-provider.h"

typedef struct
{
  GPtrArray *sdks;
} IdeSdkProviderPrivate;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeSdkProvider, ide_sdk_provider, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeSdkProvider)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  SDK_ADDED,
  SDK_REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_sdk_provider_real_sdk_added (IdeSdkProvider *self,
                                 IdeSdk         *sdk)
{
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);
  guint position;

  g_assert (IDE_IS_SDK_PROVIDER (self));
  g_assert (IDE_IS_SDK (sdk));

  _ide_sdk_set_provider (sdk, self);

  position = priv->sdks->len;
  g_ptr_array_add (priv->sdks, g_object_ref (sdk));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
ide_sdk_provider_real_sdk_removed (IdeSdkProvider *self,
                                   IdeSdk         *sdk)
{
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);
  guint position;

  g_assert (IDE_IS_SDK_PROVIDER (self));
  g_assert (IDE_IS_SDK (sdk));

  if (g_ptr_array_find (priv->sdks, sdk, &position))
    {
      g_ptr_array_remove_index (priv->sdks, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }
}

static void
ide_sdk_provider_dispose (GObject *object)
{
  IdeSdkProvider *self = (IdeSdkProvider *)object;
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);

  g_clear_pointer (&priv->sdks, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_sdk_provider_parent_class)->dispose (object);
}

static void
ide_sdk_provider_class_init (IdeSdkProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_sdk_provider_dispose;

  klass->sdk_added = ide_sdk_provider_real_sdk_added;
  klass->sdk_removed = ide_sdk_provider_real_sdk_removed;

  signals [SDK_ADDED] =
    g_signal_new ("sdk-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSdkProviderClass, sdk_added),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_SDK);
  g_signal_set_va_marshaller (signals [SDK_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);

  signals [SDK_REMOVED] =
    g_signal_new ("sdk-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSdkProviderClass, sdk_removed),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_SDK);
  g_signal_set_va_marshaller (signals [SDK_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_sdk_provider_init (IdeSdkProvider *self)
{
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);

  priv->sdks = g_ptr_array_new_with_free_func (g_object_unref);
}

void
ide_sdk_provider_sdk_added (IdeSdkProvider *self,
                            IdeSdk         *sdk)
{
  g_return_if_fail (IDE_IS_SDK_PROVIDER (self));
  g_return_if_fail (IDE_IS_SDK (sdk));

  g_signal_emit (self, signals [SDK_ADDED], 0, sdk);
}

void
ide_sdk_provider_sdk_removed (IdeSdkProvider *self,
                              IdeSdk         *sdk)
{
  g_return_if_fail (IDE_IS_SDK_PROVIDER (self));
  g_return_if_fail (IDE_IS_SDK (sdk));

  g_signal_emit (self, signals [SDK_REMOVED], 0, sdk);
}

/**
 * ide_sdk_provider_update_async:
 * @self: a #IdeSdkProvider
 * @sdk: an #IdeSdk
 * @notif: (nullable): an #IdeNotification
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Asynchronous request to update an #IdeSdk from the provider.
 */
void
ide_sdk_provider_update_async (IdeSdkProvider      *self,
                               IdeSdk              *sdk,
                               IdeNotification     *notif,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SDK_PROVIDER (self));
  g_return_if_fail (IDE_IS_SDK (sdk));
  g_return_if_fail (!notif || IDE_IS_NOTIFICATION (notif));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SDK_PROVIDER_GET_CLASS (self)->update_async (self, sdk, notif, cancellable, callback, user_data);
}

/**
 * ide_sdk_provider_update_finish:
 * @self: a #IdeSdkProvider
 * @result: a #GAsyncResult
 * @error: a location for a #GError
 *
 * Gets result of ide_sdk_provider_update_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_sdk_provider_update_finish (IdeSdkProvider  *self,
                                GAsyncResult    *result,
                                GError         **error)
{
  g_return_val_if_fail (IDE_IS_SDK_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_SDK_PROVIDER_GET_CLASS (self)->update_finish (self, result, error);
}

static GType
ide_sdk_provider_get_item_type (GListModel *model)
{
  return IDE_TYPE_SDK;
}

static guint
ide_sdk_provider_get_n_items (GListModel *model)
{
  IdeSdkProvider *self = IDE_SDK_PROVIDER (model);
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);

  return priv->sdks ? priv->sdks->len : 0;
}

static gpointer
ide_sdk_provider_get_item (GListModel *model,
                           guint       position)
{
  IdeSdkProvider *self = IDE_SDK_PROVIDER (model);
  IdeSdkProviderPrivate *priv = ide_sdk_provider_get_instance_private (self);

  if (priv->sdks == NULL || priv->sdks->len <= position)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->sdks, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_sdk_provider_get_item_type;
  iface->get_n_items = ide_sdk_provider_get_n_items;
  iface->get_item = ide_sdk_provider_get_item;
}
