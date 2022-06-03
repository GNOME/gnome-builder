/* gbp-web-browser-workspace-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-web-browser-workspace-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-webkit.h>

#include "gbp-web-browser-workspace-addin.h"

struct _GbpWebBrowserWorkspaceAddin
{
  GObject       parent_instance;
  IdeWorkspace *workspace;
};

static void workspace_addin_iface_init       (IdeWorkspaceAddinInterface  *iface);
static void web_browser_new_page_action      (GbpWebBrowserWorkspaceAddin *self,
                                              GVariant                    *param);
static void web_browser_focus_address_action (GbpWebBrowserWorkspaceAddin *self,
                                              GVariant                    *param);

IDE_DEFINE_ACTION_GROUP (GbpWebBrowserWorkspaceAddin, gbp_web_browser_workspace_addin, {
  { "new-page", web_browser_new_page_action },
  { "focus-address", web_browser_focus_address_action },
})

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWebBrowserWorkspaceAddin, gbp_web_browser_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_web_browser_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_web_browser_workspace_addin_class_init (GbpWebBrowserWorkspaceAddinClass *klass)
{
}

static void
gbp_web_browser_workspace_addin_init (GbpWebBrowserWorkspaceAddin *self)
{
}

static void
gbp_web_browser_workspace_addin_load (IdeWorkspaceAddin *addin,
                                      IdeWorkspace      *workspace)
{
  GbpWebBrowserWorkspaceAddin *self = (GbpWebBrowserWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "web-browser", G_ACTION_GROUP (self));

  IDE_EXIT;
}

static void
gbp_web_browser_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                        IdeWorkspace      *workspace)
{
  GbpWebBrowserWorkspaceAddin *self = (GbpWebBrowserWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "web-browser", NULL);

  self->workspace = NULL;

  IDE_EXIT;
}


static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_web_browser_workspace_addin_load;
  iface->unload = gbp_web_browser_workspace_addin_unload;
}

static void
web_browser_new_page_action (GbpWebBrowserWorkspaceAddin *self,
                             GVariant                    *param)
{
  g_autoptr(IdePanelPosition) position = NULL;
  IdeWebkitPage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  page = ide_webkit_page_new ();
  position = ide_panel_position_new ();

  ide_webkit_page_load_uri (page, "https://google.com/");

  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
web_browser_focus_address_action (GbpWebBrowserWorkspaceAddin *self,
                                  GVariant                    *param)
{
  IdePage *page;

  IDE_ENTRY;

  g_assert (GBP_IS_WEB_BROWSER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  if (!(page = ide_workspace_get_most_recent_page (self->workspace)))
    IDE_EXIT;

  if (!IDE_IS_WEBKIT_PAGE (page))
    IDE_EXIT;

  ide_webkit_page_focus_address (IDE_WEBKIT_PAGE (page));

  IDE_EXIT;
}
