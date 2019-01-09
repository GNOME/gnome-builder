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
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-plugins.h>
#include <libide-threading.h>
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

  GdkRGBA        added_color;
  GdkRGBA        changed_color;
};

static void
get_foreground_for_class (GtkStyleContext   *style_context,
                          const gchar       *name,
                          GdkRGBA           *rgba)
{
  GtkStateFlags state;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (name != NULL);
  g_assert (rgba != NULL);

  state = gtk_style_context_get_state (style_context);
  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, name);
  gtk_style_context_get_color (style_context, state, rgba);
  gtk_style_context_restore (style_context);
}

static void
on_tree_style_changed_cb (GbpVcsuiTreeAddin *self,
                          GtkStyleContext   *context)
{
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (GTK_IS_STYLE_CONTEXT (context));

  get_foreground_for_class (context, "vcs-added", &self->added_color);
  get_foreground_for_class (context, "vcs-changed", &self->changed_color);
}

static void
gbp_vcsui_tree_addin_load (IdeTreeAddin *addin,
                           IdeTree      *tree,
                           IdeTreeModel *model)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  GtkStyleContext *style_context;
  IdeWorkbench *workbench;
  IdeVcsMonitor *monitor;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->model = model;
  self->tree = tree;

  style_context = gtk_widget_get_style_context (GTK_WIDGET (tree));
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (on_tree_style_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_tree_style_changed_cb (self, style_context);

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
  GtkStyleContext *style_context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (tree));
  g_signal_handlers_disconnect_by_func (style_context,
                                        G_CALLBACK (on_tree_style_changed_cb),
                                        self);

  g_clear_object (&self->monitor);
  g_clear_object (&self->vcs);
  self->model = NULL;
  self->tree = NULL;
}

static void
gbp_vcsui_tree_addin_selection_changed (IdeTreeAddin *addin,
                                        IdeTreeNode  *node)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (!node || IDE_IS_TREE_NODE (node));

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
        g_object_set (cell, "foreground-rgba", &self->added_color, NULL);
      else if (status == IDE_VCS_FILE_STATUS_CHANGED)
        g_object_set (cell, "foreground-rgba", &self->changed_color, NULL);
    }
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->cell_data_func = gbp_vcsui_tree_addin_cell_data_func;
  iface->load = gbp_vcsui_tree_addin_load;
  iface->selection_changed = gbp_vcsui_tree_addin_selection_changed;
  iface->unload = gbp_vcsui_tree_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpVcsuiTreeAddin, gbp_vcsui_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_vcsui_tree_addin_class_init (GbpVcsuiTreeAddinClass *klass)
{
}

static void
gbp_vcsui_tree_addin_init (GbpVcsuiTreeAddin *self)
{
}
