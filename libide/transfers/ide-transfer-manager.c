/* ide-transfer-manager.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-transfer-manager"

#include "ide-context.h"
#include "ide-debug.h"

#include "transfers/ide-transfer.h"
#include "transfers/ide-transfer-manager.h"

#define DEFAULT_MAX_ACTIVE 1

struct _IdeTransferManager
{
  GObject    parent_instance;
  GPtrArray *transfers;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeTransferManager, ide_transfer_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_HAS_ACTIVE,
  PROP_PROGRESS,
  N_PROPS
};

enum {
  TRANSFER_COMPLETED,
  TRANSFER_FAILED,
  ALL_TRANSFERS_COMPLETED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

/**
 * ide_transfer_manager_get_has_active:
 *
 * Gets if there are active transfers.
 *
 * Returns: %TRUE if there are active transfers.
 */
gboolean
ide_transfer_manager_get_has_active (IdeTransferManager *self)
{
  g_return_val_if_fail (IDE_IS_TRANSFER_MANAGER (self), FALSE);

  for (guint i = 0; i < self->transfers->len; i++)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);

      if (ide_transfer_get_active (transfer))
        return TRUE;
    }

  return FALSE;
}

static void
ide_transfer_manager_finalize (GObject *object)
{
  IdeTransferManager *self = (IdeTransferManager *)object;

  g_clear_pointer (&self->transfers, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_transfer_manager_parent_class)->finalize (object);
}

static void
ide_transfer_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeTransferManager *self = IDE_TRANSFER_MANAGER (object);

  switch (prop_id)
    {
    case PROP_HAS_ACTIVE:
      g_value_set_boolean (value, ide_transfer_manager_get_has_active (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_transfer_manager_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_manager_class_init (IdeTransferManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_transfer_manager_finalize;
  object_class->get_property = ide_transfer_manager_get_property;

  /**
   * IdeTransferManager:has-active:
   *
   * If there are transfers active, this will be set.
   */
  properties [PROP_HAS_ACTIVE] =
    g_param_spec_boolean ("has-active",
                          "Has Active",
                          "Has Active",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTransferManager:progress:
   *
   * A double between and including 0.0 and 1.0 describing the progress of
   * all tasks.
   */
  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "Progress",
                         0.0,
                         1.0,
                         0.0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeTransferManager::all-transfers-completed:
   *
   * This signal is emitted when all of the transfers have completed or failed.
   */
  signals [ALL_TRANSFERS_COMPLETED] =
    g_signal_new ("all-transfers-completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * IdeTransferManager::transfer-completed:
   * @self: An #IdeTransferManager
   * @transfer: An #IdeTransfer
   *
   * This signal is emitted when a transfer has completed successfully.
   */
  signals [TRANSFER_COMPLETED] =
    g_signal_new ("transfer-completed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_TRANSFER);

  /**
   * IdeTransferManager::transfer-failed:
   * @self: An #IdeTransferManager
   * @transfer: An #IdeTransfer
   * @reason: (in): The reason for the failure.
   *
   * This signal is emitted when a transfer has failed to complete
   * successfully.
   */
  signals [TRANSFER_FAILED] =
    g_signal_new ("transfer-failed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, IDE_TYPE_TRANSFER, G_TYPE_ERROR);
}

static void
ide_transfer_manager_init (IdeTransferManager *self)
{
  self->transfers = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
ide_transfer_manager_notify_progress (IdeTransferManager *self,
                                      GParamSpec         *pspec,
                                      IdeTransfer        *transfer)
{
  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (IDE_IS_TRANSFER (transfer));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
}

static gboolean
ide_transfer_manager_append (IdeTransferManager *self,
                             IdeTransfer        *transfer)
{
  guint position;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TRANSFER_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TRANSFER (transfer), FALSE);

  for (guint i = 0; i < self->transfers->len; i++)
    {
      if (transfer == (IdeTransfer *)g_ptr_array_index (self->transfers, i))
        IDE_RETURN (FALSE);
    }

  g_signal_connect_object (transfer,
                           "notify::progress",
                           G_CALLBACK (ide_transfer_manager_notify_progress),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->transfers->len;
  g_ptr_array_add (self->transfers, g_object_ref (transfer));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  IDE_RETURN (TRUE);
}

void
ide_transfer_manager_cancel_all (IdeTransferManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));

  for (guint i = 0; i < self->transfers->len; i++)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);

      ide_transfer_cancel (transfer);
    }

  IDE_EXIT;
}

/**
 * ide_transfer_manager_clear:
 *
 * Removes all transfers from the manager that are completed.
 */
void
ide_transfer_manager_clear (IdeTransferManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));

  for (guint i = self->transfers->len; i > 0; i--)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i - 1);

      if (!ide_transfer_get_active (transfer))
        {
          g_ptr_array_remove_index (self->transfers, i - 1);
          g_list_model_items_changed (G_LIST_MODEL (self), i - 1, 1, 0);
        }
    }

  IDE_EXIT;
}

static GType
ide_transfer_manager_get_item_type (GListModel *model)
{
  return IDE_TYPE_TRANSFER;
}

static guint
ide_transfer_manager_get_n_items (GListModel *model)
{
  IdeTransferManager *self = (IdeTransferManager *)model;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));

  return self->transfers->len;
}

static gpointer
ide_transfer_manager_get_item (GListModel *model,
                               guint       position)
{
  IdeTransferManager *self = (IdeTransferManager *)model;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));

  if G_UNLIKELY (position >= self->transfers->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->transfers, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_transfer_manager_get_item_type;
  iface->get_n_items = ide_transfer_manager_get_n_items;
  iface->get_item = ide_transfer_manager_get_item;
}

gdouble
ide_transfer_manager_get_progress (IdeTransferManager *self)
{
  gdouble total = 0.0;

  g_return_val_if_fail (IDE_IS_TRANSFER_MANAGER (self), 0.0);

  if (self->transfers->len > 0)
    {
      guint count = 0;

      for (guint i = 0; i < self->transfers->len; i++)
        {
          IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);
          gdouble progress = ide_transfer_get_progress (transfer);

          if (ide_transfer_get_completed (transfer) || ide_transfer_get_active (transfer))
            {
              total += MAX (0.0, MIN (1.0, progress));
              count++;
            }
        }

      if (count != 0)
        total /= (gdouble)count;
    }

  return total;
}

static void
ide_transfer_manager_execute_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeTransfer *transfer = (IdeTransfer *)object;
  IdeTransferManager *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);

  if (!ide_transfer_execute_finish (transfer, result, &error))
    {
      g_signal_emit (self, signals[TRANSFER_FAILED], 0, transfer, error);
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (notify_properties);
    }
  else
    {
      g_signal_emit (self, signals[TRANSFER_COMPLETED], 0, transfer);
      g_task_return_boolean (task, TRUE);
    }

  if (!ide_transfer_manager_get_has_active (self))
    g_signal_emit (self, signals[ALL_TRANSFERS_COMPLETED], 0);

notify_properties:
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ACTIVE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);

  IDE_EXIT;
}

/**
 * ide_transfer_manager_execute_async:
 * @self: An #IdeTransferManager
 * @cancellable: (nullable): A #GCancellable
 * @callback: (nullable): A callback or %NULL
 * @user_data: user data for @callback
 *
 * This is a convenience function that will queue @transfer into the transfer
 * manager and execute callback upon completion of the transfer. The success
 * or failure #GError will be propagated to the caller via
 * ide_transfer_manager_execute_finish().
 */
void
ide_transfer_manager_execute_async (IdeTransferManager  *self,
                                    IdeTransfer         *transfer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));
  g_return_if_fail (IDE_IS_TRANSFER (transfer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_transfer_manager_execute_async);

  if (!ide_transfer_manager_append (self, transfer))
    {
      if (ide_transfer_get_active (transfer))
        {
          g_warning ("%s is already active, ignoring transfer request",
                     G_OBJECT_TYPE_NAME (transfer));
          IDE_EXIT;
        }
    }

  ide_transfer_execute_async (transfer,
                              cancellable,
                              ide_transfer_manager_execute_cb,
                              g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_transfer_manager_execute_finish (IdeTransferManager  *self,
                                     GAsyncResult        *result,
                                     GError             **error)
{
  g_return_val_if_fail (IDE_IS_TRANSFER_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
