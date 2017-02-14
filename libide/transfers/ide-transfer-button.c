/* ide-transfer-button.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-transfer-button"

#include <glib/gi18n.h>

#include "ide-debug.h"

#include "transfers/ide-transfer-button.h"
#include "transfers/ide-transfer-manager.h"
#include "util/ide-gtk.h"

typedef struct
{
  IdeTransfer  *transfer;
  GCancellable *cancellable;
} IdeTransferButtonPrivate;

enum {
  PROP_0,
  PROP_TRANSFER,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTransferButton, ide_transfer_button, GTK_TYPE_BUTTON)

static GParamSpec *properties [N_PROPS];

static void
ide_transfer_button_set_transfer (IdeTransferButton *self,
                                  IdeTransfer       *transfer)
{
  IdeTransferButtonPrivate *priv = ide_transfer_button_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_BUTTON (self));
  g_assert (!transfer || IDE_IS_TRANSFER (transfer));

  if (g_set_object (&priv->transfer, transfer))
    gtk_widget_set_sensitive (GTK_WIDGET (self), transfer != NULL);

  IDE_EXIT;
}

static void
ide_transfer_button_execute_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeTransferManager *transfer_manager = (IdeTransferManager *)object;
  g_autoptr(IdeTransferButton) self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_BUTTON (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TRANSFER_MANAGER (transfer_manager));

  ide_transfer_manager_execute_finish (transfer_manager, result, NULL);

  gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);

  IDE_EXIT;
}

static void
ide_transfer_button_clicked (GtkButton *button)
{
  IdeTransferButton *self = (IdeTransferButton *)button;
  IdeTransferButtonPrivate *priv = ide_transfer_button_get_instance_private (self);
  IdeTransferManager *transfer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_BUTTON (self));

  if (priv->transfer == NULL)
    return;

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (context == NULL)
    return;

  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

  transfer_manager = ide_context_get_transfer_manager (context);

  /* TODO: Cancellable state */
  g_clear_object (&priv->cancellable);
  priv->cancellable = g_cancellable_new ();

  ide_transfer_manager_execute_async (transfer_manager,
                                      priv->transfer,
                                      priv->cancellable,
                                      ide_transfer_button_execute_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
ide_transfer_button_finalize (GObject *object)
{
  IdeTransferButton *self = (IdeTransferButton *)object;
  IdeTransferButtonPrivate *priv = ide_transfer_button_get_instance_private (self);

  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->transfer);

  G_OBJECT_CLASS (ide_transfer_button_parent_class)->finalize (object);
}

static void
ide_transfer_button_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTransferButton *self = (IdeTransferButton *)object;
  IdeTransferButtonPrivate *priv = ide_transfer_button_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TRANSFER:
      g_value_set_object (value, priv->transfer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_button_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTransferButton *self = (IdeTransferButton *)object;

  switch (prop_id)
    {
    case PROP_TRANSFER:
      ide_transfer_button_set_transfer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_button_class_init (IdeTransferButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  object_class->finalize = ide_transfer_button_finalize;
  object_class->get_property = ide_transfer_button_get_property;
  object_class->set_property = ide_transfer_button_set_property;

  button_class->clicked = ide_transfer_button_clicked;

  properties [PROP_TRANSFER] =
    g_param_spec_object ("transfer",
                         "Transfer",
                         "Transfer",
                         IDE_TYPE_TRANSFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_transfer_button_init (IdeTransferButton *self)
{
}
