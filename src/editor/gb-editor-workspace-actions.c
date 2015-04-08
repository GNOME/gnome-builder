/* gb-editor-workspace-actions.c
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

#include "gb-editor-workspace-actions.h"
#include "gb-editor-workspace-private.h"

#define ANIMATION_DURATION_MSEC 250

static void
gb_editor_workspace_actions_show_sidebar (GSimpleAction *action,
                                          GVariant      *variant,
                                          gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  gboolean visible;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  visible = gtk_widget_get_visible (GTK_WIDGET (self->project_sidebar));

  if (!g_variant_get_boolean (variant) && visible)
    {
      ide_object_animate_full (self->project_paned,
                               IDE_ANIMATION_EASE_IN_CUBIC,
                               ANIMATION_DURATION_MSEC,
                               NULL,
                               (GDestroyNotify)gtk_widget_hide,
                               self->project_sidebar,
                               "position", 0,
                               NULL);
      g_simple_action_set_state (action, variant);
    }
  else if (g_variant_get_boolean (variant) && !visible)
    {
      gtk_paned_set_position (self->project_paned, 0);
      gtk_widget_show (GTK_WIDGET (self->project_sidebar));
      ide_object_animate (self->project_paned,
                          IDE_ANIMATION_EASE_IN_CUBIC,
                          ANIMATION_DURATION_MSEC,
                          NULL,
                          "position", 250,
                          NULL);
      g_simple_action_set_state (action, variant);
    }
}

static void
gb_editor_workspace_actions_toggle_sidebar (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GActionGroup *group;
  GAction *show_action;
  GVariant *state;

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "workspace");
  show_action = g_action_map_lookup_action (G_ACTION_MAP (group), "show-sidebar");
  state = g_action_get_state (show_action);
  gb_editor_workspace_actions_show_sidebar (G_SIMPLE_ACTION (show_action),
                                            g_variant_new_boolean (!g_variant_get_boolean (state)),
                                            user_data);
  g_variant_unref (state);
}

static void
gb_editor_workspace_tree_actions_refresh (GSimpleAction *action,
                                          GVariant      *variant,
                                          gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  gb_tree_rebuild (self->project_tree);

  /* TODO: Try to expand back to our current position */
}

static void
gb_editor_workspace_tree_actions_collapse_all_nodes (GSimpleAction *action,
                                                     GVariant      *variant,
                                                     gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (self->project_tree));
}

static const GActionEntry GbEditorWorkspaceActions[] = {
  { "show-sidebar", NULL, NULL, "false", gb_editor_workspace_actions_show_sidebar },
  { "toggle-sidebar", gb_editor_workspace_actions_toggle_sidebar },
};

static const GActionEntry GbEditorWorkspaceTreeActions[] = {
  { "refresh", gb_editor_workspace_tree_actions_refresh },
  { "collapse-all-nodes", gb_editor_workspace_tree_actions_collapse_all_nodes },
};

void
gb_editor_workspace_actions_init (GbEditorWorkspace *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(GSimpleActionGroup) tree_group = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GAction) action = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), GbEditorWorkspaceActions,
                                   G_N_ELEMENTS (GbEditorWorkspaceActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workspace", G_ACTION_GROUP (group));

  tree_group = g_simple_action_group_new ();
  settings = g_settings_new ("org.gtk.Settings.FileChooser");
  action = g_settings_create_action (settings, "sort-directories-first");
  g_action_map_add_action (G_ACTION_MAP (tree_group), action);
  g_action_map_add_action_entries (G_ACTION_MAP (tree_group), GbEditorWorkspaceTreeActions,
                                   G_N_ELEMENTS (GbEditorWorkspaceTreeActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "project-tree", G_ACTION_GROUP (tree_group));
}
