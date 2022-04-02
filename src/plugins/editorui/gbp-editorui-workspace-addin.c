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

#include <libide-editor.h>

#include "gbp-editorui-workspace-addin.h"

struct _GbpEditoruiWorkspaceAddin
{
  GObject        parent_instance;

  IdeWorkspace  *workspace;

  GtkMenuButton *indentation;
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
gbp_editorui_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;
  GMenu *menu;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;

  statusbar = ide_workspace_get_statusbar (workspace);

  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "editor-indentation");
  self->indentation = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "menu-model", menu,
                                    "direction", GTK_ARROW_UP,
                                    "visible", FALSE,
                                    NULL);
  panel_statusbar_add_suffix (statusbar, GTK_WIDGET (self->indentation));

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;
  PanelStatusbar *statusbar;

  IDE_ENTRY;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  statusbar = ide_workspace_get_statusbar (workspace);

  clear_from_statusbar (statusbar, &self->indentation);

  self->workspace = NULL;

  IDE_EXIT;
}

static void
gbp_editorui_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                           IdePage           *page)
{
  GbpEditoruiWorkspaceAddin *self = (GbpEditoruiWorkspaceAddin *)addin;

  g_assert (GBP_IS_EDITORUI_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  if (!IDE_IS_EDITOR_PAGE (page))
    page = NULL;

  gtk_widget_set_visible (GTK_WIDGET (self->indentation), page != NULL);
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
