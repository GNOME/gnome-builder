/* ide-transfer-row.c
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

#define G_LOG_DOMAIN "ide-transfer-row"

#include <dazzle.h>

#include "transfers/ide-transfer-row.h"

struct _IdeTransferRow
{
  GtkListBoxRow    parent_instance;

  IdeTransfer     *transfer;
  DzlBindingGroup *bindings;

  GtkLabel        *status;
  GtkLabel        *title;
  GtkImage        *image;
  GtkProgressBar  *progress;
  GtkButton       *cancel;
};

enum {
  PROP_0,
  PROP_TRANSFER,
  N_PROPS
};

enum {
  CANCELLED,
  N_SIGNALS
};

G_DEFINE_TYPE (IdeTransferRow, ide_transfer_row, GTK_TYPE_LIST_BOX_ROW)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_transfer_row_cancel_clicked (IdeTransferRow *self,
                                 GtkButton      *button)
{
  g_assert (IDE_IS_TRANSFER_ROW (self));
  g_assert (GTK_IS_BUTTON (button));

  g_signal_emit (self, signals [CANCELLED], 0);
}

static void
ide_transfer_row_finalize (GObject *object)
{
  IdeTransferRow *self = (IdeTransferRow *)object;

  g_clear_object (&self->transfer);
  g_clear_object (&self->bindings);

  G_OBJECT_CLASS (ide_transfer_row_parent_class)->finalize (object);
}

static void
ide_transfer_row_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTransferRow *self = IDE_TRANSFER_ROW (object);

  switch (prop_id)
    {
    case PROP_TRANSFER:
      g_value_set_object (value, ide_transfer_row_get_transfer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_row_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTransferRow *self = IDE_TRANSFER_ROW (object);

  switch (prop_id)
    {
    case PROP_TRANSFER:
      ide_transfer_row_set_transfer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_transfer_row_class_init (IdeTransferRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_transfer_row_finalize;
  object_class->get_property = ide_transfer_row_get_property;
  object_class->set_property = ide_transfer_row_set_property;

  properties [PROP_TRANSFER] =
    g_param_spec_object ("transfer",
                         "Transfer",
                         "Transfer",
                         IDE_TYPE_TRANSFER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeTransferRow::cancelled:
   *
   * This signal is emitted when the cancel button is clicked.
   */
  signals [CANCELLED] =
    g_signal_new ("cancelled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-transfer-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTransferRow, cancel);
  gtk_widget_class_bind_template_child (widget_class, IdeTransferRow, title);
  gtk_widget_class_bind_template_child (widget_class, IdeTransferRow, status);
  gtk_widget_class_bind_template_child (widget_class, IdeTransferRow, progress);
  gtk_widget_class_bind_template_child (widget_class, IdeTransferRow, image);
}

static void
ide_transfer_row_init (IdeTransferRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->cancel,
                           "clicked",
                           G_CALLBACK (ide_transfer_row_cancel_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  self->bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->bindings, "active",
                          self->progress, "visible",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->bindings, "active",
                          self->cancel, "visible",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->bindings, "title",
                          self->title, "label",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->bindings, "status",
                          self->status, "label",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->bindings, "progress",
                          self->progress, "fraction",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->bindings, "icon-name",
                          self->image, "icon-name",
                          G_BINDING_SYNC_CREATE);
}

/**
 * ide_transfer_row_get_transfer:
 *
 * Returns: (nullable) (transfer none): An #IdeTransfer or %NULL.
 */
IdeTransfer *
ide_transfer_row_get_transfer (IdeTransferRow *self)
{
  g_return_val_if_fail (IDE_IS_TRANSFER_ROW (self), NULL);

  return self->transfer;
}

void
ide_transfer_row_set_transfer (IdeTransferRow *self,
                               IdeTransfer    *transfer)
{
  g_return_if_fail (IDE_IS_TRANSFER_ROW (self));
  g_return_if_fail (!transfer || IDE_IS_TRANSFER (transfer));

  if (g_set_object (&self->transfer, transfer))
    {
      dzl_binding_group_set_source (self->bindings, transfer);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TRANSFER]);
    }
}
