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
#include "gbp-project-tree.h"
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
  g_autoptr(PanelPosition) position = NULL;

  g_assert (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  ide_pane_observe (g_object_new (GBP_TYPE_PROJECT_TREE_PANE,
                                  "title", _("Project Tree"),
                                  "icon-name", "view-list-symbolic",
                                  NULL),
                    (IdePane **)&self->pane);

  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_START);
  panel_position_set_row (position, 0);
  panel_position_set_depth (position, 0);

  ide_workspace_add_pane (workspace, IDE_PANE (self->pane), position);

  panel_widget_raise (PANEL_WIDGET (self->pane));
}

static void
gbp_project_tree_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                         IdeWorkspace      *workspace)
{
  GbpProjectTreeWorkspaceAddin *self = (GbpProjectTreeWorkspaceAddin *)addin;

  g_assert (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  ide_clear_pane ((IdePane **)&self->pane);
}

static void
gbp_project_tree_workspace_addin_page_changed (IdeWorkspaceAddin *addin,
                                               IdePage           *page)
{
  GbpProjectTreeWorkspaceAddin *self = (GbpProjectTreeWorkspaceAddin *)addin;
  GbpProjectTree *tree;
  GFile *file;

  g_assert (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  if (!IDE_IS_EDITOR_PAGE (page))
    return;

  tree = gbp_project_tree_pane_get_tree (self->pane);
  file = ide_editor_page_get_file (IDE_EDITOR_PAGE (page));

  if (tree == NULL || file == NULL)
    return;

  gbp_project_tree_reveal (tree, file);
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_project_tree_workspace_addin_load;
  iface->unload = gbp_project_tree_workspace_addin_unload;
  iface->page_changed = gbp_project_tree_workspace_addin_page_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpProjectTreeWorkspaceAddin, gbp_project_tree_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_project_tree_workspace_addin_class_init (GbpProjectTreeWorkspaceAddinClass *klass)
{
}

static void
gbp_project_tree_workspace_addin_init (GbpProjectTreeWorkspaceAddin *self)
{
}

GbpProjectTree *
gbp_project_tree_workspace_addin_get_tree (GbpProjectTreeWorkspaceAddin *self)
{
  g_return_val_if_fail (GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (self), NULL);

  return gbp_project_tree_pane_get_tree (self->pane);
}
