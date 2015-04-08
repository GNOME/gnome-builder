/* gb-project-tree-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-workspace.h"
#include "gb-editor-workspace-private.h"
#include "gb-nautilus.h"
#include "gb-tree.h"
#include "gb-widget.h"
#include "gb-workbench.h"

static void
gb_project_tree_actions_refresh (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  gb_tree_rebuild (self->project_tree);

  /* TODO: Try to expand back to our current position */
}

static void
gb_project_tree_actions_collapse_all_nodes (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (self->project_tree));
}

static void
gb_project_tree_actions_open (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GbTreeNode *selected;
  GObject *item;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  if (!(selected = gb_tree_get_selected (self->project_tree)) ||
      !(item = gb_tree_node_get_item (selected)))
    return;

  item = gb_tree_node_get_item (selected);

  if (IDE_IS_PROJECT_FILE (item))
    {
      GbWorkbench *workbench;
      GFileInfo *file_info;
      GFile *file;

      file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));
      if (!file_info)
        return;

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        return;

      file = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      if (!file)
        return;

      workbench = gb_widget_get_workbench (GTK_WIDGET (self));
      gb_workbench_open (workbench, file);
    }
}

static void
gb_project_tree_actions_open_with_editor (GSimpleAction *action,
                                          GVariant      *variant,
                                          gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GbTreeNode *selected;
  GObject *item;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  if (!(selected = gb_tree_get_selected (self->project_tree)) ||
      !(item = gb_tree_node_get_item (selected)))
    return;

  item = gb_tree_node_get_item (selected);

  if (IDE_IS_PROJECT_FILE (item))
    {
      GbWorkbench *workbench;
      GFileInfo *file_info;
      GFile *file;

      file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));
      if (!file_info)
        return;

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        return;

      file = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      if (!file)
        return;

      workbench = gb_widget_get_workbench (GTK_WIDGET (self));
      gb_workbench_open_with_editor (workbench, file);
    }
}

static void
gb_project_tree_actions_open_containing_folder (GSimpleAction *action,
                                                GVariant      *variant,
                                                gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GbTreeNode *selected;
  GObject *item;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  if (!(selected = gb_tree_get_selected (self->project_tree)) ||
      !(item = gb_tree_node_get_item (selected)))
    return;

  item = gb_tree_node_get_item (selected);

  if (IDE_IS_PROJECT_FILE (item))
    {
      GFile *file;

      file = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      if (!file)
        return;

      gb_nautilus_select_file (GTK_WIDGET (self), file, GDK_CURRENT_TIME);
    }
}

static GActionEntry GbProjectTreeActions[] = {
  { "open",                   gb_project_tree_actions_open },
  { "open-with-editor",       gb_project_tree_actions_open_with_editor },
  { "open-containing-folder", gb_project_tree_actions_open_containing_folder },
  { "refresh",                gb_project_tree_actions_refresh },
  { "collapse-all-nodes",     gb_project_tree_actions_collapse_all_nodes },
};

void
gb_project_tree_actions_init (GbEditorWorkspace *editor)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GSimpleActionGroup) actions = NULL;
  g_autoptr(GAction) action = NULL;

  settings = g_settings_new ("org.gtk.Settings.FileChooser");
  actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (actions), GbProjectTreeActions,
                                   G_N_ELEMENTS (GbProjectTreeActions), editor);

  action = g_settings_create_action (settings, "sort-directories-first");
  g_action_map_add_action (G_ACTION_MAP (actions), action);

  gtk_widget_insert_action_group (GTK_WIDGET (editor), "project-tree", G_ACTION_GROUP (actions));
}
