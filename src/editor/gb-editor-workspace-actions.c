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
#include "gb-nautilus.h"
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

static const GActionEntry GbEditorWorkspaceActions[] = {
  { "show-sidebar", NULL, NULL, "false", gb_editor_workspace_actions_show_sidebar },
  { "toggle-sidebar", gb_editor_workspace_actions_toggle_sidebar },
};

void
gb_editor_workspace_actions_init (GbEditorWorkspace *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), GbEditorWorkspaceActions,
                                   G_N_ELEMENTS (GbEditorWorkspaceActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workspace", G_ACTION_GROUP (group));
}
