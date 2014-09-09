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
  g_assert (GB_IS_EDITOR_TAB (tab));

  if (tab)
    gb_editor_tab_focus_search (GB_EDITOR_TAB (tab));
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
on_open_activate (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  GbEditorWorkspacePrivate *priv;
  GbEditorWorkspace *workspace = user_data;
  GtkFileChooserDialog *dialog;
  GtkWidget *toplevel;
  GtkWidget *suggested;
  GtkResponseType response;
  GbNotebook *notebook;
  GbTab *tab;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (workspace));

  priv = workspace->priv;

  tab = gb_multi_notebook_get_active_tab (priv->multi_notebook);
  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);

  g_assert (!tab || GB_IS_EDITOR_TAB (tab));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tab));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                         "local-only", FALSE,
                         "select-multiple", FALSE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Open"),
                         NULL);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Open"), GTK_RESPONSE_OK,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                  GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      if (!gb_editor_tab_get_is_default (GB_EDITOR_TAB (tab)))
        {
          tab = GB_TAB (gb_editor_tab_new ());
          gb_notebook_add_tab (notebook, tab);
          gtk_widget_show (GTK_WIDGET (tab));
        }

      gb_editor_tab_open_file (GB_EDITOR_TAB (tab), file);
      gb_notebook_raise_tab (notebook, tab);

      g_clear_object (&file);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_save_activate (GSimpleAction *action,
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
      gb_editor_tab_save (GB_EDITOR_TAB (tab));
    }
}

static void
on_save_as_activate (GSimpleAction *action,
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
      gb_editor_tab_save_as (GB_EDITOR_TAB (tab));
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
    { "go-to-end", on_go_to_end_activate },
    { "go-to-start", on_go_to_start_activate },
    { "open", on_open_activate },
    { "reformat", on_reformat_activate },
    { "save", on_save_activate },
    { "save-as", on_save_as_activate },
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
