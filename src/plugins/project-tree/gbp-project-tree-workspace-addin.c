/* gbp-project-tree-workspace-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-project-tree-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-project-tree-workspace-addin.h"
#include "gbp-project-tree-pane.h"

struct _GbpProjectTreeWorkspaceAddin
{
  GObject             parent_instance;
  GbpProjectTreePane *pane;
};

static void
gbp_project_tree_workspace_addin_load (IdeWorkspaceAddin *addin,
                                       IdeWorkspace      *workspace)
{
  GbpProjectTreeWorkspaceAddin *self = (GbpProjectTreeWorkspaceAddin *)addin;
  IdeEditorSidebar *sidebar;
  IdeSurface *surface;

  g_assert (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  surface = ide_workspace_get_surface_by_name (workspace, "editor");
  g_assert (IDE_IS_EDITOR_SURFACE (surface));

  sidebar = ide_editor_surface_get_sidebar (IDE_EDITOR_SURFACE (surface));
  g_assert (IDE_IS_EDITOR_SIDEBAR (sidebar));

  self->pane = g_object_new (GBP_TYPE_PROJECT_TREE_PANE,
                             "visible", TRUE,
                             NULL);
  g_signal_connect (self->pane,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->pane);
  ide_editor_sidebar_add_section (sidebar,
                                  "project-tree",
                                  _("Project Tree"),
                                  "view-list-symbolic",
                                  NULL, NULL,
                                  GTK_WIDGET (self->pane),
                                  0);
}

static void
gbp_project_tree_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                         IdeWorkspace      *workspace)
{
  GbpProjectTreeWorkspaceAddin *self = (GbpProjectTreeWorkspaceAddin *)addin;

  g_assert (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  if (self->pane != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->pane));
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_project_tree_workspace_addin_load;
  iface->unload = gbp_project_tree_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpProjectTreeWorkspaceAddin, gbp_project_tree_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_project_tree_workspace_addin_class_init (GbpProjectTreeWorkspaceAddinClass *klass)
{
}

static void
gbp_project_tree_workspace_addin_init (GbpProjectTreeWorkspaceAddin *self)
{
}
