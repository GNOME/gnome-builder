/* ide-transfers-button.c
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

#define G_LOG_DOMAIN "ide-transfers-button"

#include <dazzle.h>

#include "ide-debug.h"
#include "ide-context.h"

#include "transfers/ide-transfer.h"
#include "transfers/ide-transfer-manager.h"
#include "transfers/ide-transfer-row.h"
#include "transfers/ide-transfers-button.h"
#include "transfers/ide-transfers-progress-icon.h"
#include "util/ide-gtk.h"

struct _IdeTransfersButton
{
  GtkMenuButton             parent_instance;

  GtkPopover               *popover;
  GtkListBox               *list_box;
  IdeTransfersProgressIcon *icon;
};

G_DEFINE_TYPE (IdeTransfersButton, ide_transfers_button, GTK_TYPE_MENU_BUTTON)

static void ide_transfers_button_begin_theatrics (IdeTransfersButton *self);

GtkWidget *
ide_transfers_button_new (void)
{
  return g_object_new (IDE_TYPE_TRANSFERS_BUTTON, NULL);
}

static gboolean
begin_theatrics_from_main (gpointer user_data)
{
  g_autoptr(IdeTransfersButton) self = user_data;

  g_assert (IDE_IS_TRANSFERS_BUTTON (self));

  ide_transfers_button_begin_theatrics (self);

  return G_SOURCE_REMOVE;
}

static void
ide_transfers_button_begin_theatrics (IdeTransfersButton *self)
{
  g_autoptr(GIcon) icon = NULL;
  DzlBoxTheatric *theatric;
  GtkAllocation rect;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFERS_BUTTON (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &rect);

  if (rect.x == -1 && rect.y == -1)
    {
      /* Delay this until our widget has been mapped/realized/displayed */
      g_timeout_add (50, begin_theatrics_from_main, g_object_ref (self));
      return;
    }

  rect.x = 0;
  rect.y = 0;

  icon = g_themed_icon_new ("folder-download-symbolic");

  theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                           "alpha", 1.0,
                           "height", rect.height,
                           "icon", icon,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  dzl_object_animate_full (theatric,
                           DZL_ANIMATION_EASE_OUT_CUBIC,
                           750,
                           gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                           g_object_unref,
                           theatric,
                           "x", rect.x - 60,
                           "width", rect.width + 120,
                           "y", rect.y,
                           "height", rect.height + 120,
                           "alpha", 0.0,
                           NULL);

  IDE_EXIT;
}

static void
ide_transfers_button_cancel_clicked (IdeTransfersButton *self,
                                     IdeTransferRow     *row)
{
  IdeTransfer *transfer;

  g_assert (IDE_IS_TRANSFERS_BUTTON (self));
  g_assert (IDE_IS_TRANSFER_ROW (row));

  if (NULL != (transfer = ide_transfer_row_get_transfer (row)))
    ide_transfer_cancel (transfer);
}

static GtkWidget *
create_transfer_row (gpointer item,
                     gpointer user_data)
{
  IdeTransfersButton *self = user_data;
  IdeTransfer *transfer = item;
  IdeTransferRow *row;

  g_assert (IDE_IS_TRANSFER (transfer));
  g_assert (IDE_IS_TRANSFERS_BUTTON (self));

  row = g_object_new (IDE_TYPE_TRANSFER_ROW,
                      "selectable", FALSE,
                      "transfer", transfer,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (row,
                           "cancelled",
                           G_CALLBACK (ide_transfers_button_cancel_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (row);
}

static void
ide_transfers_button_update_visibility (IdeTransfersButton *self)
{
  IdeTransferManager *transfer_manager;
  IdeContext *context;
  gboolean visible = FALSE;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFERS_BUTTON (self));

  if (NULL != (context = ide_widget_get_context (GTK_WIDGET (self))) &&
      NULL != (transfer_manager = ide_context_get_transfer_manager (context)))
    visible = !!g_list_model_get_n_items (G_LIST_MODEL (transfer_manager));

  gtk_widget_set_visible (GTK_WIDGET (self), visible);

  IDE_EXIT;
}
static void
ide_transfers_button_context_set (GtkWidget  *widget,
                                  IdeContext *context)
{
  IdeTransfersButton *self = (IdeTransfersButton *)widget;
  IdeTransferManager *transfer_manager;

  g_assert (IDE_IS_TRANSFERS_BUTTON (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  transfer_manager = ide_context_get_transfer_manager (context);

  g_signal_connect_object (transfer_manager,
                           "all-transfers-completed",
                           G_CALLBACK (ide_transfers_button_begin_theatrics),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (transfer_manager, "progress",
                          self->icon, "progress",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (transfer_manager,
                           "items-changed",
                           G_CALLBACK (ide_transfers_button_update_visibility),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (transfer_manager),
                           create_transfer_row,
                           self,
                           NULL);

  ide_transfers_button_update_visibility (self);
}

static void
ide_transfers_button_clear (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  IdeTransfersButton *self = user_data;
  IdeTransferManager *transfer_manager;
  IdeContext *context;

  g_assert (G_IS_SIMPLE_ACTION (action));

  gtk_popover_popdown (self->popover);

  if (NULL != (context = ide_widget_get_context (GTK_WIDGET (self))) &&
      NULL != (transfer_manager = ide_context_get_transfer_manager (context)))
    ide_transfer_manager_clear (transfer_manager);
}

static void
ide_transfers_button_class_init (IdeTransfersButtonClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-transfers-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTransfersButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeTransfersButton, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeTransfersButton, icon);
}

static void
ide_transfers_button_init (IdeTransfersButton *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;
  static const GActionEntry entries[] = {
    { "clear", ide_transfers_button_clear },
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (GTK_WIDGET (self),
                                  ide_transfers_button_context_set);

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "transfers",
                                  G_ACTION_GROUP (actions));
}
