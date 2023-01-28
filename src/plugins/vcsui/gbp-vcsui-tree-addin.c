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
  IdeVcs        *vcs;
  IdeVcsMonitor *monitor;
};

static void
gbp_vcsui_tree_addin_build_node (IdeTreeAddin *addin,
                                 IdeTreeNode  *node)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  GObject *item;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));

  item = ide_tree_node_get_item (node);

  if (IDE_IS_PROJECT_FILE (item))
    {
      g_autoptr(GFile) file = ide_project_file_ref_file (IDE_PROJECT_FILE (item));
      g_autoptr(IdeVcsFileInfo) info = ide_vcs_monitor_ref_info (self->monitor, file);
      IdeTreeNodeFlags flags = ide_tree_node_get_flags (node);

      flags &= ~(IDE_TREE_NODE_FLAGS_ADDED|IDE_TREE_NODE_FLAGS_CHANGED);

      if (info != NULL)
        {
          IdeVcsFileStatus status = ide_vcs_file_info_get_status (info);

          if (status == IDE_VCS_FILE_STATUS_ADDED)
            flags |= IDE_TREE_NODE_FLAGS_ADDED;
          else if (status == IDE_VCS_FILE_STATUS_CHANGED)
            flags |= IDE_TREE_NODE_FLAGS_CHANGED;
          else if (status == IDE_VCS_FILE_STATUS_DELETED)
            flags |= IDE_TREE_NODE_FLAGS_REMOVED;
        }

      ide_tree_node_set_flags (node, flags);
    }
}

static void
gbp_vcsui_tree_addin_load (IdeTreeAddin *addin,
                           IdeTree      *tree)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  IdeVcsMonitor *monitor;
  IdeWorkbench *workbench;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  self->tree = tree;

  if ((workbench = ide_widget_get_workbench (GTK_WIDGET (tree))) &&
      (vcs = ide_workbench_get_vcs (workbench)) &&
      (monitor = ide_workbench_get_vcs_monitor (workbench)))
    {
      self->vcs = g_object_ref (vcs);
      self->monitor = g_object_ref (monitor);
    }
}

static void
gbp_vcsui_tree_addin_unload (IdeTreeAddin *addin,
                             IdeTree      *tree)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  g_clear_object (&self->monitor);
  g_clear_object (&self->vcs);

  self->tree = NULL;
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->load = gbp_vcsui_tree_addin_load;
  iface->unload = gbp_vcsui_tree_addin_unload;
  iface->build_node = gbp_vcsui_tree_addin_build_node;
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
