/* gbp-vcsui-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-vcsui-tree-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas/peas.h>

#include <libide-gui.h>
#include <libide-tree.h>
#include <libide-vcs.h>

#include "gbp-vcsui-tree-addin.h"

struct _GbpVcsuiTreeAddin
{
  GObject        parent_instance;

  IdeTree       *tree;
  IdeTreeModel  *model;
  IdeVcs        *vcs;
  IdeVcsMonitor *monitor;
};

static void
gbp_vcsui_tree_addin_load (IdeTreeAddin *addin,
                           IdeTree      *tree,
                           IdeTreeModel *model)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  IdeVcsMonitor *monitor;
  IdeWorkbench *workbench;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->model = model;
  self->tree = tree;

  if ((workbench = ide_widget_get_workbench (GTK_WIDGET (tree))) &&
      (vcs = ide_workbench_get_vcs (workbench)) &&
      (monitor = ide_workbench_get_vcs_monitor (workbench)))
    {
      self->vcs = g_object_ref (vcs);
      self->monitor = g_object_ref (monitor);
      g_signal_connect_object (self->monitor,
                               "changed",
                               G_CALLBACK (gtk_widget_queue_draw),
                               tree,
                               G_CONNECT_SWAPPED);
    }
}

static void
gbp_vcsui_tree_addin_unload (IdeTreeAddin *addin,
                             IdeTree      *tree,
                             IdeTreeModel *model)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  g_clear_object (&self->monitor);
  g_clear_object (&self->vcs);

  self->model = NULL;
  self->tree = NULL;
}

static void
gbp_vcsui_tree_addin_cell_data_func (IdeTreeAddin    *addin,
                                     IdeTreeNode     *node,
                                     GtkCellRenderer *cell)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  g_autoptr(IdeVcsFileInfo) info = NULL;
  g_autoptr(GFile) file = NULL;
  IdeProjectFile *project_file;
  IdeTreeNodeFlags flags = 0;

  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (GTK_IS_CELL_RENDERER (cell));

  if (self->monitor == NULL)
    return;

  if (!ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    return;

  project_file = ide_tree_node_get_item (node);
  file = ide_project_file_ref_file (project_file);

  if ((info = ide_vcs_monitor_ref_info (self->monitor, file)))
    {
      IdeVcsFileStatus status = ide_vcs_file_info_get_status (info);

      if (status == IDE_VCS_FILE_STATUS_ADDED)
        flags = IDE_TREE_NODE_FLAGS_ADDED;
      else if (status == IDE_VCS_FILE_STATUS_CHANGED)
        flags = IDE_TREE_NODE_FLAGS_CHANGED;

      if (flags && ide_tree_node_has_child (node))
        flags |= IDE_TREE_NODE_FLAGS_DESCENDANT;
    }

  ide_tree_node_set_flags (node, flags);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->cell_data_func = gbp_vcsui_tree_addin_cell_data_func;
  iface->load = gbp_vcsui_tree_addin_load;
  iface->unload = gbp_vcsui_tree_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVcsuiTreeAddin, gbp_vcsui_tree_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_vcsui_tree_addin_class_init (GbpVcsuiTreeAddinClass *klass)
{
}

static void
gbp_vcsui_tree_addin_init (GbpVcsuiTreeAddin *self)
{
}
