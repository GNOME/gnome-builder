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

#include <libide-editor.h>

#include "gbp-editorui-position-label.h"
#include "gbp-editorui-workspace-addin.h"

struct _GbpEditoruiWorkspaceAddin
{
  GObject                   parent_instance;

  IdeWorkspace             *workspace;
  PanelStatusbar           *statusbar;

  IdeSignalGroup           *buffer_signals;
  IdeSignalGroup           *view_signals;

  GtkMenuButton            *indentation;
  GtkLabel                 *indentation_label;

  GtkMenuButton            *position;
  GbpEditoruiPositionLabel *position_label;

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
gbp_editorui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  GMenu *menu;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
  self->statusbar = ide_workspace_get_statusbar (workspace);

  self->buffer_signals = ide_signal_group_new (IDE_TYPE_BUFFER);
  ide_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (cursor_moved_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

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

  /* Indentation status, tabs/spaces/etc */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-indent-menu");
  self->indentation_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->indentation = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "menu-model", menu,
                                    "direction", GTK_ARROW_UP,
                                    "visible", FALSE,
                                    "child", self->indentation_label,
                                    NULL);
  panel_statusbar_add_suffix (self->statusbar, GTK_WIDGET (self->indentation));

  /* Label for cursor position and jump to line/column */
  self->position_label = g_object_new (GBP_TYPE_EDITORUI_POSITION_LABEL, NULL);
  self->position = g_object_new (GTK_TYPE_MENU_BUTTON,
                                 "direction", GTK_ARROW_UP,
                                 "visible", FALSE,
                                 "child", self->position_label,
                                 NULL);
  panel_statusbar_add_suffix (self->statusbar, GTK_WIDGET (self->position));

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

  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->view_signals);

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
  IdeSourceView *view = NULL;
  IdeBuffer *buffer = NULL;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  g_clear_handle_id (&self->queued_cursor_moved, g_source_remove);

  if (!IDE_IS_EDITOR_PAGE (page))
    page = NULL;

  if (page != NULL)
    {
      view = ide_editor_page_get_view (IDE_EDITOR_PAGE (page));
      buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
    }

  ide_signal_group_set_target (self->buffer_signals, buffer);
  ide_signal_group_set_target (self->view_signals, view);

  notify_indentation_cb (self);
  update_position (self);

  gtk_widget_set_visible (GTK_WIDGET (self->indentation), page != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->position), page != NULL);
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
