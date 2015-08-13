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
#include "gb-widget.h"
#include "gb-workbench.h"

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
      gb_project_tree_save_desired_width (self->project_tree);
      egg_object_animate_full (self->project_paned,
                               EGG_ANIMATION_EASE_IN_CUBIC,
                               ANIMATION_DURATION_MSEC,
                               NULL,
                               (GDestroyNotify)gtk_widget_hide,
                               self->project_sidebar,
                               "position", 0,
                               NULL);
      g_simple_action_set_state (action, variant);
      g_settings_set_boolean (self->project_tree_settings, "show", FALSE);
    }
  else if (g_variant_get_boolean (variant) && !visible)
    {
      guint position;

      position = gb_project_tree_get_desired_width (self->project_tree);
      gtk_paned_set_position (self->project_paned, 0);
      gtk_widget_show (GTK_WIDGET (self->project_sidebar));
      egg_object_animate (self->project_paned,
                          EGG_ANIMATION_EASE_IN_CUBIC,
                          ANIMATION_DURATION_MSEC,
                          NULL,
                          "position", position,
                          NULL);
      g_simple_action_set_state (action, variant);
      g_settings_set_boolean (self->project_tree_settings, "show", TRUE);
    }
  else
    {
      g_variant_ref_sink (variant);
      g_variant_unref (variant);
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

static gboolean
focus_widget_timeout (gpointer data)
{
  gtk_widget_grab_focus (data);
  g_object_unref (data);
  return G_SOURCE_REMOVE;
}

static void
gb_editor_workspace_actions_focus_sidebar (GSimpleAction *action,
                                           GVariant      *variant,
                                           gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GActionGroup *group;
  GAction *show_action;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "workspace");
  show_action = g_action_map_lookup_action (G_ACTION_MAP (group), "show-sidebar");
  gb_editor_workspace_actions_show_sidebar (G_SIMPLE_ACTION (show_action),
                                            g_variant_new_boolean (TRUE),
                                            user_data);

  /*
   * FIXME:
   *
   * I don't like that we have to delay focusing the widget.
   * We should cleanup how sidebar toggle is managed so that things
   * can be immiediately grabbed.
   *
   * Additionally, why is 0 not enough delay? There must be something
   * else that is getting done in an idle handler causing issues.
   */
  g_timeout_add (1, focus_widget_timeout, g_object_ref (self->project_tree));
}

static void
gb_editor_workspace_actions_cpu_graph (GSimpleAction *action,
                                       GVariant      *variant,
                                       gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  /*
   * FIXME:
   *
   * Hi, you've found a hidden feature.
   *
   * I'm not sure how we want to use this long term, but I really want to start
   * watching system performance while hacking on Builder. So, for that purpose,
   * we'll shove this performance graph right in here.
   *
   * Longer term, I hope to show things like this when running applications and
   * collection runtime statistics for later analysis. But first things first.
   */

  gtk_widget_set_visible (GTK_WIDGET (self->cpu_graph),
                          !gtk_widget_get_visible (GTK_WIDGET (self->cpu_graph)));
  gtk_widget_set_visible (GTK_WIDGET (self->cpu_graph_sep),
                          !gtk_widget_get_visible (GTK_WIDGET (self->cpu_graph_sep)));
}

static void
gb_editor_workspace_actions_focus_stack (GSimpleAction *action,
                                         GVariant      *variant,
                                         gpointer       user_data)
{
  GbEditorWorkspace *self = user_data;
  GtkWidget *stack;
  GList *stacks;
  gint nth;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32));

  /* We are 1-based for this so that keybindings
   * don't look so weird mapping 1 to 0.
   */
  nth = g_variant_get_int32 (variant);
  if (nth <= 0)
    return;

  stacks = gb_view_grid_get_stacks (self->view_grid);
  stack = g_list_nth_data (stacks, nth - 1);
  g_list_free (stacks);

  if (stack != NULL)
    gtk_widget_grab_focus (stack);
}

static const GActionEntry GbEditorWorkspaceActions[] = {
  { "focus-stack", gb_editor_workspace_actions_focus_stack, "i" },
  { "focus-sidebar", gb_editor_workspace_actions_focus_sidebar },
  { "show-sidebar", NULL, NULL, "false", gb_editor_workspace_actions_show_sidebar },
  { "toggle-sidebar", gb_editor_workspace_actions_toggle_sidebar },
  { "cpu-graph", gb_editor_workspace_actions_cpu_graph },
};

void
gb_editor_workspace_actions_init (GbEditorWorkspace *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  GAction *action;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), GbEditorWorkspaceActions,
                                   G_N_ELEMENTS (GbEditorWorkspaceActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workspace", G_ACTION_GROUP (group));

  action = g_action_map_lookup_action (G_ACTION_MAP (group), "show-sidebar");
  g_assert (G_IS_SIMPLE_ACTION (action));

  if (g_settings_get_boolean (self->project_tree_settings, "show"))
    {
      guint position;

      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (TRUE));
      position = g_settings_get_int (self->project_tree_settings, "width");
      gtk_paned_set_position (self->project_paned, position);
      gtk_widget_show (GTK_WIDGET (self->project_sidebar));
    }
}
