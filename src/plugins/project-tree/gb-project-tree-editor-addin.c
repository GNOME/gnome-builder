/* gb-project-tree-editor-addin.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#include "gb-project-tree.h"
#include "gb-project-tree-editor-addin.h"

struct _GbProjectTreeEditorAddin
{
  GObject        parent_instance;

  IdeEditorView *view;
};

static void editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbProjectTreeEditorAddin, gb_project_tree_editor_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_VIEW_ADDIN,
                                               editor_view_addin_iface_init))

static void
gb_project_tree_editor_addin_class_init (GbProjectTreeEditorAddinClass *klass)
{
}

static void
gb_project_tree_editor_addin_init (GbProjectTreeEditorAddin *self)
{
}

static void
gb_project_tree_editor_addin_reveal (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  GbProjectTreeEditorAddin *self = user_data;
  IdeWorkbench *workbench;
  GbProjectTree *tree;
  IdeBuffer *buffer;
  IdeFile *ifile;
  GFile *file;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_PROJECT_TREE_EDITOR_ADDIN (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->view));
  g_assert (IDE_IS_WORKBENCH (workbench));

  tree = g_object_get_data (G_OBJECT (workbench), "GB_PROJECT_TREE");
  g_assert (GB_IS_PROJECT_TREE (tree));

  buffer = ide_editor_view_get_buffer (self->view);
  g_assert (IDE_IS_BUFFER (buffer));

  ifile = ide_buffer_get_file (buffer);
  g_assert (IDE_IS_FILE (ifile));

  file = ide_file_get_file (ifile);
  g_assert (!file || G_IS_FILE (file));

  if (G_IS_FILE (file))
    gb_project_tree_reveal (tree, file, TRUE, FALSE);
}

static void
gb_project_tree_editor_addin_load (IdeEditorViewAddin *addin,
                                   IdeEditorView      *view)
{
  GbProjectTreeEditorAddin *self = (GbProjectTreeEditorAddin *)addin;
  GSimpleActionGroup *group;
  static const GActionEntry entries[] = {
    { "reveal", gb_project_tree_editor_addin_reveal },
  };

  g_assert (GB_IS_PROJECT_TREE_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  self->view = view;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "project-tree", G_ACTION_GROUP (group));
  g_object_unref (group);
}

static void
gb_project_tree_editor_addin_unload (IdeEditorViewAddin *addin,
                                     IdeEditorView      *view)
{
  GbProjectTreeEditorAddin *self = (GbProjectTreeEditorAddin *)addin;

  g_assert (GB_IS_PROJECT_TREE_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  gtk_widget_insert_action_group (GTK_WIDGET (view), "project-tree", NULL);

  self->view = NULL;
}

static void
editor_view_addin_iface_init (IdeEditorViewAddinInterface *iface)
{
  iface->load = gb_project_tree_editor_addin_load;
  iface->unload = gb_project_tree_editor_addin_unload;
}
