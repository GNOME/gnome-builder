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

#include "gbp-editorui-workspace-addin.h"

struct _GbpEditoruiWorkspaceAddin
{
  GObject          parent_instance;

  IdeWorkspace    *workspace;
  PanelStatusbar  *statusbar;

  IdeSignalGroup  *view_signals;

  GtkMenuButton   *indentation;
  GtkLabel        *indentation_label;
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

  gtk_widget_set_visible (GTK_WIDGET (self->indentation), view != NULL);
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

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editorui-indent-menu");
  self->indentation_label = g_object_new (GTK_TYPE_LABEL, NULL);
  self->indentation = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "menu-model", menu,
                                    "direction", GTK_ARROW_UP,
                                    "visible", FALSE,
                                    "child", self->indentation_label,
                                    NULL);
  panel_statusbar_add_suffix (self->statusbar, GTK_WIDGET (self->indentation));

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

  g_clear_object (&self->view_signals);

  clear_from_statusbar (self->statusbar, &self->indentation);

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

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  if (!IDE_IS_EDITOR_PAGE (page))
    page = NULL;

  if (page != NULL)
    view = ide_editor_page_get_view (IDE_EDITOR_PAGE (page));

  ide_signal_group_set_target (self->view_signals, view);

  notify_indentation_cb (self);
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
