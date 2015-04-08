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
action_set (GActionGroup *group,
            const gchar  *action_name,
            const gchar  *first_param,
            ...)
{
  GAction *action;
  va_list args;

  g_assert (G_IS_ACTION_GROUP (group));
  g_assert (G_IS_ACTION_MAP (group));

  action = g_action_map_lookup_action (G_ACTION_MAP (group), action_name);
  g_assert (G_IS_SIMPLE_ACTION (action));

  va_start (args, first_param);
  g_object_set_valist (G_OBJECT (action), first_param, args);
  va_end (args);
}

static gboolean
is_project_file_not_directory (GObject *object)
{
  GFileInfo *info;

  g_assert (!object || G_IS_OBJECT (object));

  return (IDE_IS_PROJECT_FILE (object) &&
          (info = ide_project_file_get_file_info (IDE_PROJECT_FILE (object))) &&
          (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY));
}

static void
gb_project_tree_actions_update_actions (GbEditorWorkspace *editor)
{
  GActionGroup *group;
  GbTreeNode *selection;
  GObject *item = NULL;

  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_WORKSPACE (editor));
  group = gtk_widget_get_action_group (GTK_WIDGET (editor), "project-tree");
  g_assert (G_IS_SIMPLE_ACTION_GROUP (group));

  selection = gb_tree_get_selected (editor->project_tree);
  if (selection != NULL)
    item = gb_tree_node_get_item (selection);

  action_set (group, "open",
              "enabled", is_project_file_not_directory (item),
              NULL);
  action_set (group, "open-with-editor",
              "enabled", is_project_file_not_directory (item),
              NULL);
  action_set (group, "open-containing-folder",
              "enabled", IDE_IS_PROJECT_FILE (item),
              NULL);

  IDE_EXIT;
}

static void
gb_project_tree_actions__notify_selection (GbTree            *tree,
                                           GParamSpec        *pspec,
                                           GbEditorWorkspace *editor)
{
  g_assert (GB_IS_TREE (tree));
  g_assert (GB_IS_EDITOR_WORKSPACE (editor));

  gb_project_tree_actions_update_actions (editor);
}

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

static void
gb_project_tree_actions_show_icons (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GbEditorWorkspace *editor = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (editor));

  gb_tree_set_show_icons (editor->project_tree, g_variant_get_boolean (variant));
  g_simple_action_set_state (action, variant);
}

static GActionEntry GbProjectTreeActions[] = {
  { "collapse-all-nodes",     gb_project_tree_actions_collapse_all_nodes },
  { "open",                   gb_project_tree_actions_open },
  { "open-containing-folder", gb_project_tree_actions_open_containing_folder },
  { "open-with-editor",       gb_project_tree_actions_open_with_editor },
  { "refresh",                gb_project_tree_actions_refresh },
  { "show-icons",             NULL, NULL, "false", gb_project_tree_actions_show_icons },
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

  g_signal_connect (editor->project_tree,
                    "notify::selection",
                    G_CALLBACK (gb_project_tree_actions__notify_selection),
                    editor);

  gb_project_tree_actions_update_actions (editor);
}
