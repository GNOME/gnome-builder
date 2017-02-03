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
  guint      max_active;
  GPtrArray *transfers;
};

static void list_model_iface_init                 (GListModelInterface *iface);
static void ide_transfer_manager_pump             (IdeTransferManager  *self);
static void ide_transfer_manager_execute_complete (IdeTransferManager *self,
                                                   GTask              *task,
                                                   const GError       *reason);

G_DEFINE_TYPE_EXTENDED (IdeTransferManager, ide_transfer_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_HAS_ACTIVE,
  PROP_MAX_ACTIVE,
  PROP_PROGRESS,
  N_PROPS
};

enum {
  TRANSFER_COMPLETED,
  TRANSFER_FAILED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

#define GET_BOOLEAN(obj,name)     (NULL != g_object_get_data(G_OBJECT(obj), name))
#define SET_BOOLEAN(obj,name,val) (g_object_set_data(G_OBJECT(obj), name, GINT_TO_POINTER(val)))

static void
transfer_cancel (IdeTransfer *transfer)
{
  GCancellable *cancellable;

  g_assert (IDE_IS_TRANSFER (transfer));

  cancellable = g_object_get_data (G_OBJECT (transfer), "IDE_TRANSFER_CANCELLABLE");
  if (G_IS_CANCELLABLE (cancellable) && !g_cancellable_is_cancelled (cancellable))
    g_cancellable_cancel (cancellable);
}

static gboolean
transfer_get_active (IdeTransfer *transfer)
{
  return GET_BOOLEAN (transfer, "IDE_TRANSFER_ACTIVE");
}

static void
transfer_set_active (IdeTransfer *transfer,
                     gboolean     active)
{
  SET_BOOLEAN (transfer, "IDE_TRANSFER_ACTIVE", active);
}

static void
transfer_set_completed (IdeTransfer *transfer,
                        gboolean     completed)
{
  SET_BOOLEAN (transfer, "IDE_TRANSFER_COMPLETED", completed);
}

static guint
ide_transfer_manager_count_active (IdeTransferManager *self)
{
  guint active = 0;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));

  for (guint i = 0; i < self->transfers->len; i++)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);

      if (transfer_get_active (transfer) && !ide_transfer_has_completed (transfer))
        active++;
    }

  return active;
}

static void
ide_transfer_manager_execute_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeTransfer *transfer = (IdeTransfer *)object;
  g_autoptr(IdeTransferManager) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (IDE_IS_TRANSFER (transfer));

  transfer_set_completed (transfer, TRUE);

  if (!ide_transfer_execute_finish (transfer, result, &error))
    {
      IdeContext *context;

      context = ide_object_get_context (IDE_OBJECT (self));
      ide_context_warning (context, "%s", error->message);

      g_signal_emit (self, signals [TRANSFER_FAILED], 0, transfer, error);
    }
  else
    g_signal_emit (self, signals [TRANSFER_COMPLETED], 0, transfer);

  ide_transfer_manager_pump (self);

  IDE_EXIT;
}

static void
ide_transfer_manager_begin (IdeTransferManager *self,
                            IdeTransfer        *transfer)
{
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (IDE_IS_TRANSFER (transfer));

  transfer_set_active (transfer, TRUE);

  cancellable = g_cancellable_new ();

  g_object_set_data_full (G_OBJECT (transfer),
                          "IDE_TRANSFER_CANCELLABLE",
                          cancellable,
                          g_object_unref);

  ide_transfer_execute_async (transfer,
                              cancellable,
                              ide_transfer_manager_execute_cb,
                              g_object_ref (self));

  IDE_EXIT;
}

static void
ide_transfer_manager_pump (IdeTransferManager *self)
{
  guint active;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));

  active = ide_transfer_manager_count_active (self);

  if (active < self->max_active)
    {
      for (guint i = 0; i < self->transfers->len; i++)
        {
          IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);

          if (!transfer_get_active (transfer))
            {
              active++;
              ide_transfer_manager_begin (self, transfer);
              if (active >= self->max_active)
                break;
            }
        }
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ACTIVE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);

  IDE_EXIT;
}

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

      if (transfer_get_active (transfer) && !ide_transfer_has_completed (transfer))
        return TRUE;
    }

  return FALSE;
}

guint
ide_transfer_manager_get_max_active (IdeTransferManager *self)
{
  g_return_val_if_fail (IDE_IS_TRANSFER_MANAGER (self), 0);

  return self->max_active;
}

void
ide_transfer_manager_set_max_active (IdeTransferManager *self,
                                     guint               max_active)
{
  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));

  if (self->max_active != max_active)
    {
      self->max_active = max_active;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_ACTIVE]);
      ide_transfer_manager_pump (self);
    }
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

    case PROP_MAX_ACTIVE:
      g_value_set_uint (value, ide_transfer_manager_get_max_active (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, ide_transfer_manager_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  IdeTransferManager *self = IDE_TRANSFER_MANAGER (object);

  switch (prop_id)
    {
    case PROP_MAX_ACTIVE:
      ide_transfer_manager_set_max_active (self, g_value_get_uint (value));
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
  object_class->set_property = ide_transfer_manager_set_property;

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
   * IdeTransferManager:max-active:
   *
   * Sets the max number of transfers to have active at one time.
   * Set to zero for a sensible default.
   */
  properties [PROP_MAX_ACTIVE] =
    g_param_spec_uint ("max-active",
                       "Max Active",
                       "Max Active",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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
  self->max_active = DEFAULT_MAX_ACTIVE;
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

void
ide_transfer_manager_queue (IdeTransferManager *self,
                            IdeTransfer        *transfer)
{
  guint position;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));
  g_return_if_fail (IDE_IS_TRANSFER (transfer));

  g_signal_connect_object (transfer,
                           "notify::progress",
                           G_CALLBACK (ide_transfer_manager_notify_progress),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->transfers->len;
  g_ptr_array_add (self->transfers, g_object_ref (transfer));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
  ide_transfer_manager_pump (self);

  IDE_EXIT;
}

void
ide_transfer_manager_cancel_all (IdeTransferManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));

  for (guint i = 0; i < self->transfers->len; i++)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);

      transfer_cancel (transfer);
    }

  IDE_EXIT;
}

void
ide_transfer_manager_cancel (IdeTransferManager *self,
                             IdeTransfer        *transfer)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));
  g_return_if_fail (IDE_IS_TRANSFER (transfer));

  transfer_cancel (transfer);

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

      if (ide_transfer_has_completed (transfer))
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

  if (self->transfers->len == 0)
    return 0.0;

  for (guint i = 0; i < self->transfers->len; i++)
    {
      IdeTransfer *transfer = g_ptr_array_index (self->transfers, i);
      gdouble progress;

      progress = ide_transfer_get_progress (transfer);
      total += MAX (0.0, MIN (1.0, progress));
    }

  return total / (gdouble)self->transfers->len;
}

static void
ide_transfer_manager_execute_transfer_completed (IdeTransferManager *self,
                                                 IdeTransfer        *transfer,
                                                 GTask              *task)
{
  IdeTransfer *task_data;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (G_IS_TASK (task));

  task_data = g_task_get_task_data (task);

  if (task_data == transfer)
    ide_transfer_manager_execute_complete (self, task, NULL);
}

static void
ide_transfer_manager_execute_transfer_failed (IdeTransferManager *self,
                                              IdeTransfer        *transfer,
                                              const GError       *reason,
                                              GTask              *task)
{
  IdeTransfer *task_data;

  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (reason != NULL);
  g_assert (G_IS_TASK (task));

  task_data = g_task_get_task_data (task);

  if (task_data == transfer)
    ide_transfer_manager_execute_complete (self, task, reason);
}

static void
ide_transfer_manager_execute_complete (IdeTransferManager *self,
                                       GTask              *task,
                                       const GError       *reason)
{
  g_assert (IDE_IS_TRANSFER_MANAGER (self));
  g_assert (G_IS_TASK (task));

  g_signal_handlers_disconnect_by_func (self,
                                        G_CALLBACK (ide_transfer_manager_execute_transfer_completed),
                                        task);

  g_signal_handlers_disconnect_by_func (self,
                                        G_CALLBACK (ide_transfer_manager_execute_transfer_failed),
                                        task);

  if (reason != NULL)
    g_task_return_error (task, g_error_copy (reason));
  else
    g_task_return_boolean (task, TRUE);
}

/**
 * ide_transfer_manager_execute_async:
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

  g_return_if_fail (IDE_IS_TRANSFER_MANAGER (self));
  g_return_if_fail (IDE_IS_TRANSFER (transfer));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_transfer_manager_execute_async);
  g_task_set_task_data (task, g_object_ref (transfer), g_object_unref);

  g_signal_connect_data (self,
                         "transfer-completed",
                         G_CALLBACK (ide_transfer_manager_execute_transfer_completed),
                         g_object_ref (task),
                         (GClosureNotify)g_object_unref,
                         0);

  g_signal_connect_data (self,
                         "transfer-failed",
                         G_CALLBACK (ide_transfer_manager_execute_transfer_failed),
                         g_object_ref (task),
                         (GClosureNotify)g_object_unref,
                         0);

  ide_transfer_manager_queue (self, transfer);
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
