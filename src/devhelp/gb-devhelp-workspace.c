/* gb-devhelp-workspace.c
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

#include <devhelp/devhelp.h>
#include <glib/gi18n.h>

#include "gb-devhelp-tab.h"
#include "gb-devhelp-workspace.h"
#include "gb-multi-notebook.h"
#include "gb-notebook.h"

struct _GbDevhelpWorkspacePrivate
{
  DhBookManager      *book_manager;
  GSimpleActionGroup *actions;

  GtkPaned           *paned;
  DhSidebar          *sidebar;
  GbMultiNotebook    *multi_notebook;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDevhelpWorkspace,
                            gb_devhelp_workspace,
                            GB_TYPE_WORKSPACE)

static void
update_show_tabs (GbDevhelpWorkspace *workspace)
{
  GbDevhelpWorkspacePrivate *priv;
  gboolean show_tabs;
  GList *tabs;

  g_assert (GB_IS_DEVHELP_WORKSPACE (workspace));

  priv = workspace->priv;

  tabs = gb_multi_notebook_get_all_tabs (priv->multi_notebook);
  show_tabs = (g_list_length (tabs) > 1);
  g_list_free (tabs);

  if (!show_tabs && gb_multi_notebook_get_n_notebooks (priv->multi_notebook))
    show_tabs = TRUE;

  gb_multi_notebook_set_show_tabs (priv->multi_notebook, show_tabs);
}

static void
on_close_tab_activated (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  GbDevhelpWorkspace *workspace = user_data;
  GbTab *tab;

  g_return_if_fail (GB_IS_DEVHELP_WORKSPACE (workspace));

  tab = gb_multi_notebook_get_active_tab (workspace->priv->multi_notebook);
  if (tab)
    gb_tab_close (tab);
}

static void
on_new_tab_activated (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  GbDevhelpWorkspacePrivate *priv;
  GbDevhelpWorkspace *workspace = user_data;
  GbNotebook *notebook;
  GbTab *tab;
  gint page = -1;

  g_return_if_fail (GB_IS_DEVHELP_WORKSPACE (workspace));

  priv = workspace->priv;

  tab = g_object_new (GB_TYPE_DEVHELP_TAB,
                      "title", _ ("Empty Page"),
                      "visible", TRUE,
                      NULL);
  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);
  gb_notebook_add_tab (notebook, GB_TAB (tab));

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &page,
                           NULL);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);

  update_show_tabs (GB_DEVHELP_WORKSPACE (workspace));
}

static void
gb_devhelp_workspace_link_selected (GbDevhelpWorkspace *workspace,
                                    DhLink             *link_,
                                    DhSidebar          *sidebar)
{
  GbDevhelpWorkspacePrivate *priv;
  const gchar *uri;
  GbNotebook *notebook;
  GbTab *tab;

  g_return_if_fail (GB_IS_DEVHELP_WORKSPACE (workspace));
  g_return_if_fail (link_);
  g_return_if_fail (DH_IS_SIDEBAR (sidebar));

  priv = workspace->priv;

  tab = gb_multi_notebook_get_active_tab (priv->multi_notebook);
  uri = dh_link_get_uri (link_);

  if (!tab)
    {
      notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);
      tab = g_object_new (GB_TYPE_DEVHELP_TAB,
                          "visible", TRUE,
                          NULL);
      gb_notebook_add_tab (notebook, tab);
    }

  gb_devhelp_tab_set_uri (GB_DEVHELP_TAB (tab), uri);
}

static void
on_n_notebooks_changed (GbMultiNotebook    *mnb,
                        GParamSpec         *pspec,
                        GbDevhelpWorkspace *workspace)
{
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));
  g_return_if_fail (GB_IS_DEVHELP_WORKSPACE (workspace));

  update_show_tabs (workspace);
}

static void
gb_devhelp_workspace_constructed (GObject *object)
{
  GbDevhelpWorkspacePrivate *priv = GB_DEVHELP_WORKSPACE (object)->priv;
  static const GActionEntry action_entries[] = {
     { "close-tab", on_close_tab_activated },
     { "new-tab", on_new_tab_activated },
  };

  g_return_if_fail (GB_IS_DEVHELP_WORKSPACE (object));

  dh_book_manager_populate (priv->book_manager);

  priv->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (priv->actions),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   object);
}

static GActionGroup *
gb_devhelp_workspace_get_actions (GbWorkspace *workspace)
{
   GbDevhelpWorkspacePrivate *priv = GB_DEVHELP_WORKSPACE (workspace)->priv;

   g_assert (GB_IS_DEVHELP_WORKSPACE (workspace));

   return G_ACTION_GROUP (priv->actions);
}

static void
gb_devhelp_workspace_finalize (GObject *object)
{
  GbDevhelpWorkspacePrivate *priv;

  priv = GB_DEVHELP_WORKSPACE (object)->priv;

  g_clear_object (&priv->actions);
  g_clear_object (&priv->book_manager);

  G_OBJECT_CLASS (gb_devhelp_workspace_parent_class)->finalize (object);
}

static void
gb_devhelp_workspace_class_init (GbDevhelpWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbWorkspaceClass *workspace_class = GB_WORKSPACE_CLASS (klass);

  object_class->constructed = gb_devhelp_workspace_constructed;
  object_class->finalize = gb_devhelp_workspace_finalize;

  workspace_class->get_actions = gb_devhelp_workspace_get_actions;
}

static void
gb_devhelp_workspace_init (GbDevhelpWorkspace *workspace)
{
  GbDevhelpWorkspacePrivate *priv;
  GbDevhelpTab *tab;
  GbNotebook *notebook;

  workspace->priv = gb_devhelp_workspace_get_instance_private (workspace);

  priv = workspace->priv;

  priv->book_manager = dh_book_manager_new ();

  priv->paned = g_object_new (GTK_TYPE_PANED,
                              "expand", TRUE,
                              "orientation", GTK_ORIENTATION_HORIZONTAL,
                              "position", 300,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (workspace), GTK_WIDGET (priv->paned));

  priv->sidebar = g_object_new (DH_TYPE_SIDEBAR,
                                "book-manager", priv->book_manager,
                                "orientation", GTK_ORIENTATION_VERTICAL,
                                "visible", TRUE,
                                NULL);
  g_signal_connect_object (priv->sidebar,
                           "link-selected",
                           G_CALLBACK (gb_devhelp_workspace_link_selected),
                           workspace,
                           G_CONNECT_SWAPPED);
  gtk_paned_add1 (priv->paned, GTK_WIDGET (priv->sidebar));

  priv->multi_notebook = g_object_new (GB_TYPE_MULTI_NOTEBOOK,
                                       "group-name", "GB_DEVHELP_WORKSPACE",
                                       "show-tabs", FALSE,
                                       "visible", TRUE,
                                       NULL);
  g_signal_connect (priv->multi_notebook,
                    "notify::n-notebooks",
                    G_CALLBACK (on_n_notebooks_changed),
                    workspace);
  gtk_paned_add2 (priv->paned, GTK_WIDGET (priv->multi_notebook));

  tab = g_object_new (GB_TYPE_DEVHELP_TAB,
                      "title", _ ("Empty Page"),
                      "visible", TRUE,
                      NULL);
  notebook = gb_multi_notebook_get_active_notebook (priv->multi_notebook);
  gb_notebook_add_tab (notebook, GB_TAB (tab));
}
