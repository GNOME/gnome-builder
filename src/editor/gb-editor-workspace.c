/* gb-editor-workspace.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-commands.h"
#include "gb-editor-workspace.h"
#include "gb-editor-workspace-private.h"

enum {
  PROP_0,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorWorkspace, gb_editor_workspace, GB_TYPE_WORKSPACE)

void
gb_editor_workspace_open (GbEditorWorkspace *workspace,
                          GFile             *file)
{
  GbEditorTab *tab;
  GbNotebook *notebook;
  gint page;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));
  g_return_if_fail (G_IS_FILE (file));

  notebook = gb_multi_notebook_get_active_notebook (workspace->priv->multi_notebook);

  tab = g_object_new (GB_TYPE_EDITOR_TAB,
                      "visible", TRUE,
                      NULL);
  gb_notebook_add_tab (notebook, GB_TAB (tab));

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &page,
                           NULL);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);

  gb_editor_tab_open_file (tab, file);

  gtk_widget_grab_focus (GTK_WIDGET (tab));
}

static GActionGroup *
gb_editor_workspace_get_actions (GbWorkspace * workspace)
{
  g_return_val_if_fail (GB_IS_EDITOR_WORKSPACE (workspace), NULL);

  return G_ACTION_GROUP (GB_EDITOR_WORKSPACE (workspace)->priv->actions);
}

static void
gb_editor_workspace_grab_focus (GtkWidget *widget)
{
  GbEditorWorkspace *workspace = GB_EDITOR_WORKSPACE (widget);
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);

  if (tab)
    gtk_widget_grab_focus (GTK_WIDGET (tab));
}

static void
gb_editor_workspace_finalize (GObject *object)
{
  GbEditorWorkspacePrivate *priv = GB_EDITOR_WORKSPACE (object)->priv;

  g_clear_object (&priv->actions);
  g_clear_pointer (&priv->command_map, g_hash_table_unref);

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbWorkspaceClass *workspace_class = GB_WORKSPACE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_editor_workspace_finalize;

  workspace_class->get_actions = gb_editor_workspace_get_actions;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;
}

static void
gb_editor_workspace_init (GbEditorWorkspace *workspace)
{
  workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  workspace->priv->actions = g_simple_action_group_new ();
  workspace->priv->command_map = g_hash_table_new (g_str_hash, g_str_equal);

  /*
   * TODO: make this be done with GtkBuilder.
   */
  workspace->priv->multi_notebook =
      g_object_new (GB_TYPE_MULTI_NOTEBOOK,
                    "visible", TRUE,
                    "group-name", "GB_EDITOR_WORKSPACE",
                    NULL);
  gtk_container_add (GTK_CONTAINER (workspace),
                     GTK_WIDGET (workspace->priv->multi_notebook));

  gb_editor_commands_init (workspace);
}
