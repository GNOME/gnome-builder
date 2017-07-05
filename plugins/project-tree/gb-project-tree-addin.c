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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gb-project-tree.h"
#include "gb-project-tree-addin.h"
#include "gb-project-tree-resources.h"

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

struct _GbProjectTreeAddin
{
  GObject    parent_instance;

  DzlTree   *tree;
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
gb_project_tree_addin_load (IdeWorkbenchAddin *addin,
                            IdeWorkbench      *workbench)
{
  GbProjectTreeAddin *self = (GbProjectTreeAddin *)addin;
  IdeEditorSidebar *sidebar;
  IdePerspective *editor;
  GtkWidget *scroller;

  g_assert (IDE_IS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  editor = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (editor != NULL);

  sidebar = ide_editor_perspective_get_sidebar (IDE_EDITOR_PERSPECTIVE (editor));
  g_assert (IDE_IS_EDITOR_SIDEBAR (sidebar));

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "visible", TRUE,
                           NULL);

  self->tree = g_object_new (GB_TYPE_PROJECT_TREE,
                             "headers-visible", FALSE,
                             "level-indentation", 22,
                             "visible", TRUE,
                             NULL);
  g_signal_connect (self->tree,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->tree);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->tree), "project-tree");
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (self->tree));

  ide_editor_sidebar_add_section (sidebar,
                                  "project-tree-view",
                                  _("Project Tree"),
                                  "view-list-symbolic",
                                  NULL, NULL,
                                  GTK_WIDGET (scroller),
                                  0);

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

  if (self->tree != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->tree));

  g_object_set_data (G_OBJECT (workbench), "GB_PROJECT_TREE", NULL);
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
