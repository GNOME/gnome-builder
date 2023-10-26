/* gbp-symbol-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-symbol-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-symbol-popover.h"
#include "gbp-symbol-workspace-addin.h"
#include "gbp-symbol-util.h"

#define NEAREST_SCOPE_DELAY_MSEC 500
#define SETTLING_DELAY_MSEC      50
#define SYMBOL_TREE_DELAY_MSEC   1000

struct _GbpSymbolWorkspaceAddin
{
  GObject           parent_instance;

  IdeWorkspace     *workspace;
  PanelStatusbar   *statusbar;

  GtkMenuButton    *menu_button;
  GtkLabel         *menu_label;
  GtkImage         *menu_image;
  GbpSymbolPopover *popover;

  GSignalGroup     *buffer_signals;
  guint             nearest_scope_timeout_source;
  guint             nearest_scope_settling_source;
  guint             symbol_tree_timeout_source;
};

static void
focus_action (GbpSymbolWorkspaceAddin *self,
              GVariant                *param)
{
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  if (gtk_widget_get_visible (GTK_WIDGET (self->menu_button)))
    {
      gtk_menu_button_popup (self->menu_button);
      gtk_widget_grab_focus (GTK_WIDGET (self->popover));
    }
}

IDE_DEFINE_ACTION_GROUP (GbpSymbolWorkspaceAddin, gbp_symbol_workspace_addin, {
  { "focus", focus_action },
});

static void
gbp_symbol_workspace_addin_set_symbol (GbpSymbolWorkspaceAddin *self,
                                       IdeSymbol               *symbol)
{
  g_autofree char *truncated = NULL;
  const char *label = NULL;
  const char *icon_name = NULL;
  const char *nl;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (!symbol || IDE_IS_SYMBOL (symbol));

  if (symbol != NULL)
    {
      icon_name = ide_symbol_kind_get_icon_name (ide_symbol_get_kind (symbol));
      label = ide_symbol_get_name (symbol);

      if (ide_str_empty0 (label))
        label = NULL;
    }

  if (label == NULL)
    {
      gtk_label_set_label (self->menu_label, _("Select Symbol…"));
      gtk_image_set_from_icon_name (self->menu_image, NULL);
      gtk_widget_hide (GTK_WIDGET (self->menu_image));
      IDE_EXIT;
    }

  if ((nl = strchr (label, '\n')))
    label = truncated = g_strndup (label, nl - label);

  gtk_label_set_label (self->menu_label, label);
  gtk_image_set_from_icon_name (self->menu_image, icon_name);
  gtk_widget_set_visible (GTK_WIDGET (self->menu_image), icon_name != NULL);

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_find_nearest_scope_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GbpSymbolWorkspaceAddin) self = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  if (!(symbol = gbp_symbol_find_nearest_scope_finish (buffer, result, &error)))
    {
      if (!ide_error_ignore (error))
        g_warning ("Failed to get symbol at location: %s", error->message);
      IDE_GOTO (failure);
    }

  if ((gpointer)buffer != _g_signal_group_get_target (self->buffer_signals))
    IDE_GOTO (failure);

  gbp_symbol_workspace_addin_set_symbol (self, symbol);
  gtk_widget_show (GTK_WIDGET (self->menu_button));

  IDE_EXIT;

failure:

  /* Raced against another query or cleanup and lost, just bail */
  if (_g_signal_group_get_target (self->buffer_signals) != buffer)
    IDE_EXIT;

  gbp_symbol_workspace_addin_set_symbol (self, NULL);
  gtk_widget_show (GTK_WIDGET (self->menu_button));

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_get_symbol_tree_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GbpSymbolWorkspaceAddin) self = user_data;
  g_autoptr(IdeSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  if (!(tree = gbp_symbol_get_symbol_tree_finish (buffer, result, &error)))
    {
      if (!ide_error_ignore (error))
        g_warning ("Failed to get symbol tree: %s", error->message);
      IDE_GOTO (failure);
    }

  if ((gpointer)buffer != _g_signal_group_get_target (self->buffer_signals))
    IDE_GOTO (failure);

  gbp_symbol_popover_set_symbol_tree (self->popover, tree);

  IDE_EXIT;

failure:

  /* Raced against another query and lost, just bail */
  if ((gpointer)buffer != _g_signal_group_get_target (self->buffer_signals))
    IDE_EXIT;

  gbp_symbol_popover_set_symbol_tree (self->popover, NULL);

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_update_nearest_scope (GbpSymbolWorkspaceAddin *self,
                                                 IdeBuffer               *buffer)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!ide_buffer_has_symbol_resolvers (buffer))
    {
      gbp_symbol_workspace_addin_set_symbol (self, NULL);
      gtk_widget_hide (GTK_WIDGET (self->menu_button));
      IDE_EXIT;
    }

  gbp_symbol_find_nearest_scope_async (buffer,
                                       NULL,
                                       gbp_symbol_workspace_addin_find_nearest_scope_cb,
                                       g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_update_symbol_tree (GbpSymbolWorkspaceAddin *self,
                                               IdeBuffer               *buffer)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!ide_buffer_has_symbol_resolvers (buffer))
    {
      gbp_symbol_popover_set_symbol_tree (self->popover, NULL);
      IDE_EXIT;
    }

  gbp_symbol_get_symbol_tree_async (buffer,
                                    NULL,
                                    gbp_symbol_workspace_addin_get_symbol_tree_cb,
                                    g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_buffer_bind_cb (GbpSymbolWorkspaceAddin *self,
                                           IdeBuffer               *buffer,
                                           GSignalGroup            *signal_group)
{
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  gbp_symbol_workspace_addin_update_nearest_scope (self, buffer);
  gbp_symbol_workspace_addin_update_symbol_tree (self, buffer);
}

static gboolean
gbp_symbol_workspace_addin_nearest_scope_timeout (gpointer data)
{
  GbpSymbolWorkspaceAddin *self = data;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  self->nearest_scope_timeout_source = 0;

  if ((buffer = _g_signal_group_get_target (self->buffer_signals)))
    gbp_symbol_workspace_addin_update_nearest_scope (self, buffer);
  else
    gtk_widget_hide (GTK_WIDGET (self->menu_button));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static gboolean
gbp_symbol_workspace_addin_buffer_cursor_moved_cb (gpointer data)
{
  GbpSymbolWorkspaceAddin *self = data;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  self->nearest_scope_settling_source = 0;

  if (self->nearest_scope_timeout_source == 0)
    self->nearest_scope_timeout_source = g_timeout_add (NEAREST_SCOPE_DELAY_MSEC,
                                                        gbp_symbol_workspace_addin_nearest_scope_timeout,
                                                        self);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_symbol_workspace_addin_buffer_cursor_moved_settling_cb (GbpSymbolWorkspaceAddin *self,
                                                            IdeBuffer               *buffer)
{
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_clear_handle_id (&self->nearest_scope_settling_source, g_source_remove);
  self->nearest_scope_settling_source = g_timeout_add (SETTLING_DELAY_MSEC,
                                                       gbp_symbol_workspace_addin_buffer_cursor_moved_cb,
                                                       self);
}

static gboolean
gbp_symbol_workspace_addin_symbol_tree_timeout (gpointer data)
{
  GbpSymbolWorkspaceAddin *self = data;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));

  self->symbol_tree_timeout_source = 0;

  if ((buffer = _g_signal_group_get_target (self->buffer_signals)))
    gbp_symbol_workspace_addin_update_symbol_tree (self, buffer);
  else
    gbp_symbol_popover_set_symbol_tree (self->popover, NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_symbol_workspace_addin_buffer_changed_cb (GbpSymbolWorkspaceAddin *self,
                                              IdeBuffer               *buffer)
{
  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_clear_handle_id (&self->symbol_tree_timeout_source, g_source_remove);
  self->symbol_tree_timeout_source = g_timeout_add (SYMBOL_TREE_DELAY_MSEC,
                                                    gbp_symbol_workspace_addin_symbol_tree_timeout,
                                                    self);
}

static void
gbp_symbol_workspace_addin_load (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpSymbolWorkspaceAddin *self = (GbpSymbolWorkspaceAddin *)addin;
  GtkBox *box;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
  self->statusbar = ide_workspace_get_statusbar (workspace);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 6,
                      NULL);
  self->menu_image = g_object_new (GTK_TYPE_IMAGE,
                                   "icon-name", "lang-function-symbolic",
                                   NULL);
  gtk_box_append (box, GTK_WIDGET (self->menu_image));
  self->menu_label = g_object_new (GTK_TYPE_LABEL,
                                   "label", _("Select Symbol…"),
                                   "xalign", 0.0f,
                                   "ellipsize", PANGO_ELLIPSIZE_END,
                                   "tooltip-text", _("Select Symbol (Ctrl+Shift+K)"),
                                   NULL);
  gtk_box_append (box, GTK_WIDGET (self->menu_label));
  self->popover = GBP_SYMBOL_POPOVER (gbp_symbol_popover_new ());
  self->menu_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "child", box,
                                    "direction", GTK_ARROW_UP,
                                    "popover", self->popover,
                                    "visible", FALSE,
                                    NULL);

  panel_statusbar_add_suffix (self->statusbar, 20000, GTK_WIDGET (self->menu_button));

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpSymbolWorkspaceAddin *self = (GbpSymbolWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (PANEL_IS_STATUSBAR (self->statusbar));
  g_assert (workspace == self->workspace);

  g_signal_group_set_target (self->buffer_signals, NULL);

  g_clear_handle_id (&self->nearest_scope_timeout_source, g_source_remove);
  g_clear_handle_id (&self->symbol_tree_timeout_source, g_source_remove);

  panel_statusbar_remove (self->statusbar, GTK_WIDGET (self->menu_button));

  self->menu_button = NULL;
  self->menu_label = NULL;
  self->menu_image = NULL;
  self->workspace = NULL;
  self->statusbar = NULL;

  IDE_EXIT;
}

static void
gbp_symbol_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                         IdePage           *page)
{
  GbpSymbolWorkspaceAddin *self = (GbpSymbolWorkspaceAddin *)addin;
  IdeBuffer *buffer = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  gbp_symbol_popover_set_symbol_tree (self->popover, NULL);
  gbp_symbol_workspace_addin_set_symbol (self, NULL);
  gtk_widget_hide (GTK_WIDGET (self->menu_button));

  if (IDE_IS_EDITOR_PAGE (page))
    buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));

  g_signal_group_set_target (self->buffer_signals, buffer);

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_symbol_workspace_addin_load;
  iface->unload = gbp_symbol_workspace_addin_unload;
  iface->page_changed = gbp_symbol_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSymbolWorkspaceAddin, gbp_symbol_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_symbol_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_symbol_workspace_addin_finalize (GObject *object)
{
  GbpSymbolWorkspaceAddin *self = (GbpSymbolWorkspaceAddin *)object;

  g_clear_object (&self->buffer_signals);
  g_clear_handle_id (&self->nearest_scope_settling_source, g_source_remove);

  G_OBJECT_CLASS (gbp_symbol_workspace_addin_parent_class)->finalize (object);
}

static void
gbp_symbol_workspace_addin_class_init (GbpSymbolWorkspaceAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_symbol_workspace_addin_finalize;
}

static void
gbp_symbol_workspace_addin_init (GbpSymbolWorkspaceAddin *self)
{
  self->buffer_signals = g_signal_group_new (IDE_TYPE_BUFFER);
  g_signal_connect_object (self->buffer_signals,
                           "bind",
                           G_CALLBACK (gbp_symbol_workspace_addin_buffer_bind_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (gbp_symbol_workspace_addin_buffer_cursor_moved_settling_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (self->buffer_signals,
                                   "changed",
                                   G_CALLBACK (gbp_symbol_workspace_addin_buffer_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

}

GListModel *
gbp_symbol_workspace_addin_get_model (GbpSymbolWorkspaceAddin *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self), NULL);

  if (self->popover == NULL)
    return NULL;

  return gbp_symbol_popover_get_model (self->popover);
}

IdeBuffer *
gbp_symbol_workspace_addin_get_buffer (GbpSymbolWorkspaceAddin *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_WORKSPACE_ADDIN (self), NULL);

  return _g_signal_group_get_target (self->buffer_signals);
}
