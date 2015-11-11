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

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

struct _GbProjectTreeAddin
{
  GObject parent_instance;
};

G_DEFINE_TYPE_EXTENDED (GbProjectTreeAddin, gb_project_tree_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gb_project_tree_addin_load (IdeWorkbenchAddin *addin,
                            IdeWorkbench      *workbench)
{
  IdePerspective *editor;
  GtkWidget *pane;
  GtkWidget *tree;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  pane = ide_layout_get_left_pane (IDE_LAYOUT (editor));
  tree = g_object_new (GB_TYPE_PROJECT_TREE,
                       "headers-visible", FALSE,
                       "visible", TRUE,
                       NULL);
  ide_layout_pane_add_page (IDE_LAYOUT_PANE (pane), tree, _("Project Tree"), "folder-symbolic");
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_project_tree_addin_load;
}

static void
gb_project_tree_addin_class_init (GbProjectTreeAddinClass *klass)
{
}

static void
gb_project_tree_addin_init (GbProjectTreeAddin *addin)
{
}
