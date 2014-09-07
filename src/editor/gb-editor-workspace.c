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

#include "gb-editor-tab.h"
#include "gb-editor-workspace.h"
#include "gb-multi-notebook.h"
#include "gb-notebook.h"

struct _GbEditorWorkspacePrivate
{
  GSimpleActionGroup *actions;
  GbMultiNotebook    *multi_notebook;
};

enum {
  PROP_0,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorWorkspace, gb_editor_workspace, GB_TYPE_WORKSPACE)

static GActionGroup *
gb_editor_workspace_get_actions (GbWorkspace * workspace)
{
  g_return_val_if_fail (GB_IS_EDITOR_WORKSPACE (workspace), NULL);

  return G_ACTION_GROUP (GB_EDITOR_WORKSPACE (workspace)->priv->actions);
}

static void
gb_editor_workspace_new_tab (GbWorkspace *workspace)
{
  GbEditorWorkspacePrivate *priv;
  GbNotebook *notebook;
  GbTab *tab;
  gint page;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = GB_EDITOR_WORKSPACE (workspace)->priv;

  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);
  tab = g_object_new (GB_TYPE_EDITOR_TAB,
                      "visible", TRUE,
                      NULL);
  gb_notebook_add_tab (notebook, tab);

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &page,
                           NULL);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
}

static void
gb_editor_workspace_find (GbWorkspace *workspace)
{
  GbEditorWorkspacePrivate *priv;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = GB_EDITOR_WORKSPACE (workspace)->priv;

  tab = gb_multi_notebook_get_active_tab (priv->multi_notebook);

  if (tab)
    gb_editor_tab_set_show_find (GB_EDITOR_TAB (tab), TRUE);
}

static void
on_reformat_activate (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);

  if (tab)
    {
      g_assert (GB_IS_EDITOR_TAB (tab));
      gb_editor_tab_reformat (GB_EDITOR_TAB (tab));
    }
}

static void
on_go_to_start_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);

  if (tab)
    {
      g_assert (GB_IS_EDITOR_TAB (tab));
      gb_editor_tab_go_to_start (GB_EDITOR_TAB (tab));
    }
}

static void
on_go_to_end_activate (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  GbEditorWorkspace *workspace = user_data;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);

  if (tab)
    {
      g_assert (GB_IS_EDITOR_TAB (tab));
      gb_editor_tab_go_to_end (GB_EDITOR_TAB (tab));
    }
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

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbWorkspaceClass *workspace_class = GB_WORKSPACE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_editor_workspace_finalize;

  workspace_class->find = gb_editor_workspace_find;
  workspace_class->new_tab = gb_editor_workspace_new_tab;
  workspace_class->get_actions = gb_editor_workspace_get_actions;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;
}

static void
gb_editor_workspace_init (GbEditorWorkspace *workspace)
{
  static const GActionEntry action_entries[] = {
    { "reformat", on_reformat_activate },
    { "go-to-start", on_go_to_start_activate },
    { "go-to-end", on_go_to_end_activate },
  };
  GbEditorWorkspacePrivate *priv;
  GbNotebook *notebook;
  GbTab *tab;

  priv = workspace->priv = gb_editor_workspace_get_instance_private (workspace);

  priv->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (priv->actions),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   workspace);

  gb_workspace_set_icon_name (GB_WORKSPACE (workspace), "text-x-generic");
  gb_workspace_set_title (GB_WORKSPACE (workspace), _ ("Editor"));

  priv->multi_notebook = g_object_new (GB_TYPE_MULTI_NOTEBOOK,
                                       "visible", TRUE,
                                       "group-name", "GB_EDITOR_WORKSPACE",
                                       NULL);
  gtk_container_add (GTK_CONTAINER (workspace),
                     GTK_WIDGET (priv->multi_notebook));

  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);

  tab = g_object_new (GB_TYPE_EDITOR_TAB,
                      "visible", TRUE,
                      NULL);
  gb_notebook_add_tab (notebook, tab);
}
