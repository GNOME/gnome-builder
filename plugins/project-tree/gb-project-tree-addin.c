/* gb-project-tree-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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
 */

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-project-tree.h"
#include "gb-project-tree-addin.h"
#include "gb-project-tree-resources.h"

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

struct _GbProjectTreeAddin
{
  GObject    parent_instance;

  IdeTree   *tree;
  GtkWidget *panel;
};

G_DEFINE_TYPE_EXTENDED (GbProjectTreeAddin, gb_project_tree_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gb_project_tree_addin_context_set (GtkWidget  *widget,
                                   IdeContext *context)
{
  GbProjectTree *project_tree = (GbProjectTree *)widget;

  g_assert (GB_IS_PROJECT_TREE (project_tree));
  g_assert (!context || IDE_IS_CONTEXT (context));

  gb_project_tree_set_context (project_tree, context);
}

static void
gb_project_tree_addin_grid_empty (GbProjectTreeAddin *self,
                                  IdeLayoutGrid      *grid)
{
  GtkWidget *layout;
  GtkWidget *pane;

  g_assert (GB_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_GRID (grid));

  layout = gtk_widget_get_ancestor (GTK_WIDGET (grid), IDE_TYPE_LAYOUT);
  g_assert (layout != NULL);

  pane = pnl_dock_bin_get_left_edge (PNL_DOCK_BIN (layout));
  g_assert (pane != NULL);

  pnl_dock_revealer_set_reveal_child (PNL_DOCK_REVEALER (pane), TRUE);
}

static void
gb_project_tree_addin_load (IdeWorkbenchAddin *addin,
                            IdeWorkbench      *workbench)
{
  GbProjectTreeAddin *self = (GbProjectTreeAddin *)addin;
  IdePerspective *editor;
  GtkWidget *pane;
  GtkWidget *content;
  GtkWidget *scroller;
  GtkWidget *grid;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (editor != NULL);

  pane = pnl_dock_bin_get_left_edge (PNL_DOCK_BIN (editor));
  g_assert (pane != NULL);

  content = pnl_dock_bin_get_center_widget (PNL_DOCK_BIN (editor));
  g_assert (content != NULL);

  grid = ide_widget_find_child_typed (content, IDE_TYPE_LAYOUT_GRID);
  g_assert (grid != NULL);

  g_signal_connect_object (grid,
                           "empty",
                           G_CALLBACK (gb_project_tree_addin_grid_empty),
                           self,
                           G_CONNECT_SWAPPED);

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "visible", TRUE,
                           NULL);
  self->tree = g_object_new (GB_TYPE_PROJECT_TREE,
                             "headers-visible", FALSE,
                             "visible", TRUE,
                             NULL);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (self->tree));

  self->panel = g_object_new (PNL_TYPE_DOCK_WIDGET,
                              "expand", TRUE,
                              "title", _("Project"),
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self->panel), GTK_WIDGET (scroller));
  gtk_container_add (GTK_CONTAINER (pane), GTK_WIDGET (self->panel));

  ide_widget_set_context_handler (self->tree, gb_project_tree_addin_context_set);

  g_object_set_data (G_OBJECT (workbench), "GB_PROJECT_TREE", self->tree);
}

static void
gb_project_tree_addin_unload (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  GbProjectTreeAddin *self = (GbProjectTreeAddin *)addin;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_widget_destroy (self->panel);
  self->panel = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_project_tree_addin_load;
  iface->unload = gb_project_tree_addin_unload;
}

static void
gb_project_tree_addin_class_init (GbProjectTreeAddinClass *klass)
{
  g_resources_register (gb_project_tree_get_resource ());
}

static void
gb_project_tree_addin_init (GbProjectTreeAddin *addin)
{
}
