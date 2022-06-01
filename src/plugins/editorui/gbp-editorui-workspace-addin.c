/* gbp-editorui-workspace-addin.c
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

#define G_LOG_DOMAIN "gbp-editorui-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-editor.h>

#include "ide-workspace-private.h"

#include "gbp-editorui-position-label.h"
#include "gbp-editorui-workspace-addin.h"

struct _GbpEditoruiWorkspaceAddin
{
  GObject                   parent_instance;

  IdeWorkspace             *workspace;
  PanelStatusbar           *statusbar;

  GSimpleActionGroup       *actions;

  IdeBindingGroup          *buffer_bindings;
  IdeSignalGroup           *buffer_signals;
  IdeSignalGroup           *view_signals;

  GtkMenuButton            *indentation;
  GtkLabel                 *indentation_label;

  GtkMenuButton            *line_ends;
  GtkLabel                 *line_ends_label;

  GtkMenuButton            *position;
  GbpEditoruiPositionLabel *position_label;

  GtkMenuButton            *encoding;
  GtkLabel                 *encoding_label;

  GtkLabel                 *mode_label;

  GSettings                *editor_settings;

  guint                     queued_cursor_moved;
};

#define clear_from_statusbar(s,w) clear_from_statusbar(s, (GtkWidget **)w)

static void
(clear_from_statusbar) (PanelStatusbar  *statusbar,
                        GtkWidget      **widget)
{
  if (*widget)
    {
      panel_statusbar_remove (statusbar, *widget);
      *widget = NULL;
    }
}

static gboolean
newline_type_to_label (GBinding     *binding,
                       const GValue *from_value,
                       GValue       *to_value,
                       gpointer      user_data)
{
  GtkSourceNewlineType newline_type = g_value_get_enum (from_value);

  switch (newline_type)
    {
    default:
    case GTK_SOURCE_NEWLINE_TYPE_LF:
      g_value_set_static_string (to_value, "LF");
      return TRUE;

    case GTK_SOURCE_NEWLINE_TYPE_CR:
      g_value_set_static_string (to_value, "CR");
      return TRUE;

    case GTK_SOURCE_NEWLINE_TYPE_CR_LF:
      g_value_set_static_string (to_value, "CR/LF");
      return TRUE;
    }
}

static void
notify_overwrite_cb (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = ide_signal_group_get_target (self->view_signals)))
    {
      gboolean overwrite = gtk_text_view_get_overwrite (GTK_TEXT_VIEW (view));

      if (overwrite)
        gtk_label_set_label (self->mode_label, "OVR");
      else
        gtk_label_set_label (self->mode_label, "INS");
    }
}

static void
notify_indentation_cb (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = ide_signal_group_get_target (self->view_signals)))
    {
      g_autofree char *label = NULL;
      gboolean insert_spaces_instead_of_tabs;
      guint tab_width;
      int indent_width;

      g_object_get (view,
                    "tab-width", &tab_width,
                    "indent-width", &indent_width,
                    "insert-spaces-instead-of-tabs", &insert_spaces_instead_of_tabs,
                    NULL);

      if (indent_width <= 0)
        indent_width = tab_width;

      label = g_strdup_printf ("%s %u:%u",
                               insert_spaces_instead_of_tabs ?  _("Space") : _("Tab"),
                               indent_width, tab_width);
      gtk_label_set_label (self->indentation_label, label);
    }
}

static void
update_position (GbpEditoruiWorkspaceAddin *self)
{
  IdeSourceView *view;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if ((view = ide_signal_group_get_target (self->view_signals)))
    {
      guint line, column;

      ide_source_view_get_visual_position (view, &line, &column);
      gbp_editorui_position_label_update (self->position_label, line, column);
    }
}

static gboolean
update_position_idle (gpointer data)
{
  GbpEditoruiWorkspaceAddin *self = data;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  self->queued_cursor_moved = 0;
  update_position (self);
  return G_SOURCE_REMOVE;
}

static void
cursor_moved_cb (GbpEditoruiWorkspaceAddin *self)
{
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if (self->queued_cursor_moved)
    return;

  self->queued_cursor_moved = g_idle_add (update_position_idle, self);
}

static void
open_in_new_frame (GSimpleAction *action,
                   GVariant      *param,
                   gpointer       user_data)
{
  GbpEditoruiWorkspaceAddin *self = user_data;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  IDE_EXIT;
}

static void
open_in_new_workspace (GSimpleAction *action,
                       GVariant      *param,
                       gpointer       user_data)
{
  GbpEditoruiWorkspaceAddin *self = user_data;
  g_autoptr(IdePanelPosition) position = NULL;
  IdeEditorWorkspace *workspace;
  IdeWorkbench *workbench;
  IdePage *page;
  IdePage *split;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  if (!(page = ide_workspace_get_most_recent_page (self->workspace)))
    IDE_EXIT;

  if (!(split = ide_page_create_split (page)))
    IDE_EXIT;

  workbench = ide_workspace_get_workbench (self->workspace);

  workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  position = ide_panel_position_new ();
  ide_workspace_add_page (IDE_WORKSPACE (workspace), IDE_PAGE (split), position);

  gtk_window_present (GTK_WINDOW (workspace));

  IDE_EXIT;
}

static void
new_workspace (GSimpleAction *action,
               GVariant      *param,
               gpointer       user_data)
{
  GbpEditoruiWorkspaceAddin *self = user_data;
  IdeEditorWorkspace *workspace;
  IdeWorkbench *workbench;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));

  workbench = ide_workspace_get_workbench (self->workspace);
  workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

  gtk_window_present (GTK_WINDOW (workspace));

  IDE_EXIT;
}

static void
new_file_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeWorkspace) workspace = user_data;
  g_autoptr(IdePanelPosition) position = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *page;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    {
      g_warning ("Failed to create new buffer: %s", error->message);
      IDE_EXIT;
    }

  page = ide_editor_page_new (buffer);
  position = ide_panel_position_new ();
  ide_workspace_add_page (workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
new_file (GSimpleAction *action,
          GVariant      *param,
          gpointer       user_data)
{
  GbpEditoruiWorkspaceAddin *self = user_data;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  context = ide_workspace_get_context (self->workspace);
  bufmgr = ide_buffer_manager_from_context (context);

  ide_buffer_manager_load_file_async (bufmgr,
                                      NULL,
                                      IDE_BUFFER_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      new_file_cb,
                                      g_object_ref (self->workspace));

  IDE_EXIT;
}

static const GActionEntry actions[] = {
  { "open-in-new-frame", open_in_new_frame },
  { "open-in-new-workspace", open_in_new_workspace },
  { "new-file", new_file },
  { "new-workspace", new_workspace },
};

static void
gbp_editorui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  g_autoptr(GMenuModel) encoding_menu = NULL;
  GtkPopover *popover;
  GMenu *menu;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
  self->statusbar = ide_workspace_get_statusbar (workspace);

  self->encoding_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->line_ends_label = g_object_new (GTK_TYPE_LABEL, NULL);

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "editorui",
                                  G_ACTION_GROUP (self->actions));

  self->buffer_signals = ide_signal_group_new (IDE_TYPE_BUFFER);
  ide_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (cursor_moved_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->buffer_bindings = ide_binding_group_new ();
  ide_binding_group_bind (self->buffer_bindings, "charset",
                          self->encoding_label, "label",
                          G_BINDING_SYNC_CREATE);
  ide_binding_group_bind_full (self->buffer_bindings, "newline-type",
                               self->line_ends_label, "label",
                               G_BINDING_SYNC_CREATE,
                               newline_type_to_label,
                               NULL, NULL, NULL);

  self->view_signals = ide_signal_group_new (IDE_TYPE_SOURCE_VIEW);
  ide_signal_group_connect_object (self->view_signals,
                                   "notify::indent-width",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->view_signals,
                                   "notify::tab-width",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->view_signals,
                                   "notify::insert-spaces-instead-of-tabs",
                                   G_CALLBACK (notify_indentation_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  ide_signal_group_connect_object (self->view_signals,
                                   "notify::overwrite",
                                   G_CALLBACK (notify_overwrite_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  /* Encoding */
  encoding_menu = ide_editor_encoding_menu_new ("editorui.encoding");
  self->encoding = g_object_new (GTK_TYPE_MENU_BUTTON,
                                 "menu-model", encoding_menu,
                                 "direction", GTK_ARROW_UP,
                                 "visible", FALSE,
                                 "child", self->encoding_label,
                                 NULL);
  panel_statusbar_add_suffix (self->statusbar, 1000, GTK_WIDGET (self->encoding));

  /* Line ending */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-line-ends-menu");
  self->line_ends = g_object_new (GTK_TYPE_MENU_BUTTON,
                                  "menu-model", menu,
                                  "direction", GTK_ARROW_UP,
                                  "visible", FALSE,
                                  "child", self->line_ends_label,
                                  NULL);
  panel_statusbar_add_suffix (self->statusbar, 1001, GTK_WIDGET (self->line_ends));

  self->mode_label = g_object_new (GTK_TYPE_LABEL,
                                   "label", "INS",
                                   "width-chars", 4,
                                   "visible", FALSE,
                                   NULL);
  panel_statusbar_add_suffix (self->statusbar, 1002, GTK_WIDGET (self->mode_label));

  /* Indentation status, tabs/spaces/etc */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-indent-menu");
  self->indentation_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->indentation = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "menu-model", menu,
                                    "direction", GTK_ARROW_UP,
                                    "visible", FALSE,
                                    "child", self->indentation_label,
                                    NULL);
  panel_statusbar_add_suffix (self->statusbar, 1003, GTK_WIDGET (self->indentation));

  /* Label for cursor position and jump to line/column */
  popover = g_object_new (IDE_TYPE_ENTRY_POPOVER,
                          "button-text", _("Go"),
                          "title", _("Go to Line"),
                          NULL);
  self->position_label = g_object_new (GBP_TYPE_EDITORUI_POSITION_LABEL, NULL);
  self->position = g_object_new (GTK_TYPE_MENU_BUTTON,
                                 "direction", GTK_ARROW_UP,
                                 "visible", FALSE,
                                 "child", self->position_label,
                                 "popover", popover,
                                 NULL);
  panel_statusbar_add_suffix (self->statusbar, 1004, GTK_WIDGET (self->position));

  self->editor_settings = g_settings_new ("org.gnome.builder.editor");

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "editorui", NULL);

  g_clear_object (&self->buffer_bindings);
  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->view_signals);
  g_clear_object (&self->editor_settings);

  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);

  clear_from_statusbar (self->statusbar, &self->indentation);
  clear_from_statusbar (self->statusbar, &self->position);

  self->indentation_label = NULL;
  self->position_label = NULL;

  self->workspace = NULL;
  self->statusbar = NULL;

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                           IdePage           *page)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  g_autofree char *keybindings = NULL;
  IdeSourceView *view = NULL;
  IdeBuffer *buffer = NULL;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);

  /* Remove now invalid actions */
  g_action_map_remove_action (G_ACTION_MAP (self->actions), "encoding");
  g_action_map_remove_action (G_ACTION_MAP (self->actions), "newline-type");
  g_action_map_remove_action (G_ACTION_MAP (self->actions), "indent-width");
  g_action_map_remove_action (G_ACTION_MAP (self->actions), "tab-width");

  if (!IDE_IS_EDITOR_PAGE (page))
    page = NULL;

  if (page != NULL)
    {
      g_autoptr(GPropertyAction) encoding_action = NULL;
      g_autoptr(GPropertyAction) newline_action = NULL;
      g_autoptr(GPropertyAction) indent_width = NULL;
      g_autoptr(GPropertyAction) tab_width = NULL;

      view = ide_editor_page_get_view (IDE_EDITOR_PAGE (page));
      buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));

      encoding_action = g_property_action_new ("encoding", buffer, "charset");
      newline_action = g_property_action_new ("newline-type", buffer, "newline-type");
      indent_width = g_property_action_new ("indent-width", view, "indent-width");
      tab_width = g_property_action_new ("tab-width", view, "tab-width");

      g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (encoding_action));
      g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (newline_action));
      g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (tab_width));
      g_action_map_add_action (G_ACTION_MAP (self->actions), G_ACTION (indent_width));
    }

  ide_binding_group_set_source (self->buffer_bindings, buffer);
  ide_signal_group_set_target (self->buffer_signals, buffer);
  ide_signal_group_set_target (self->view_signals, view);

  notify_overwrite_cb (self);
  notify_indentation_cb (self);
  update_position (self);

  keybindings = g_settings_get_string (self->editor_settings, "keybindings");

  gtk_widget_set_visible (GTK_WIDGET (self->indentation), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->line_ends), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->position), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->encoding), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->mode_label), page != NULL && !ide_str_equal0 (keybindings, "vim"));
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_editorui_workspace_addin_load;
  iface->unload = gbp_editorui_workspace_addin_unload;
  iface->page_changed = gbp_editorui_workspace_addin_page_changed;
}

G_DEFINE_TYPE_WITH_CODE (GbpEditoruiWorkspaceAddin, gbp_editorui_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_editorui_workspace_addin_class_init (GbpEditoruiWorkspaceAddinClass *klass)
{
}

static void
gbp_editorui_workspace_addin_init (GbpEditoruiWorkspaceAddin *self)
{
}
