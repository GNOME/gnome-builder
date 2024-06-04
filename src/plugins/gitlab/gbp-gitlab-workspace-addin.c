/* gbp-gitlab-workspace-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gitlab-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <gitlab-glib.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-gitlab-workbench-addin.h"
#include "gbp-gitlab-workspace-addin.h"

struct _GbpGitlabWorkspaceAddin
{
  GObject                  parent_instance;
  IdeWorkspace            *workspace;
  GbpGitlabWorkbenchAddin *workbench_addin;
};

static void action_open_project (GbpGitlabWorkspaceAddin *self,
                                 GVariant                *param);

IDE_DEFINE_ACTION_GROUP (GbpGitlabWorkspaceAddin, gbp_gitlab_workspace_addin, {
  { "open-project", action_open_project },
})

static void
gbp_gitlab_workspace_addin_load (IdeWorkspaceAddin *addin,
                                 IdeWorkspace      *workspace)
{
  GbpGitlabWorkspaceAddin *self = (GbpGitlabWorkspaceAddin *)addin;
  GbpGitlabWorkbenchAddin *workbench_addin;
  GtkMenuButton *menu_button;
  IdeHeaderBar *header_bar;
  IdeWorkbench *workbench;
  GMenu *menu;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace));

  self->workspace = workspace;

  workbench = ide_workspace_get_workbench (workspace);
  workbench_addin = GBP_GITLAB_WORKBENCH_ADDIN (ide_workbench_addin_find_by_module_name (workbench, "gitlab"));

  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (workbench_addin));

  g_set_object (&self->workbench_addin, workbench_addin);

  header_bar = ide_workspace_get_header_bar (workspace);
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "gitlab-menu");
  menu_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                              "always-show-arrow", TRUE,
                              "icon-name", "gitlab-symbolic",
                              "menu-model", menu,
                              NULL);

  /* Only show button if/when we know the project is backed by
   * a gitlab instance we can possibly talk to.
   */
  g_object_bind_property (workbench_addin, "enabled",
                          menu_button, "visible",
                          G_BINDING_SYNC_CREATE);

  ide_header_bar_add (header_bar, IDE_HEADER_BAR_POSITION_LEFT_OF_CENTER, 0, GTK_WIDGET (menu_button));
}

static void
gbp_gitlab_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpGitlabWorkspaceAddin *self = (GbpGitlabWorkspaceAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GITLAB_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  g_clear_object (&self->workbench_addin);

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_gitlab_workspace_addin_load;
  iface->unload = gbp_gitlab_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitlabWorkspaceAddin, gbp_gitlab_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_gitlab_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_gitlab_workspace_addin_class_init (GbpGitlabWorkspaceAddinClass *klass)
{
}

static void
gbp_gitlab_workspace_addin_init (GbpGitlabWorkspaceAddin *self)
{
}

static void
action_open_project (GbpGitlabWorkspaceAddin *self,
                     GVariant                *param)
{
  g_autofree char *url = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GITLAB_WORKSPACE_ADDIN (self));
  g_assert (GBP_IS_GITLAB_WORKBENCH_ADDIN (self->workbench_addin));

  if ((url = gbp_gitlab_workbench_addin_get_project_url (self->workbench_addin)))
    ide_gtk_show_uri_on_window (GTK_WINDOW (self->workspace), url, 0, NULL);

  IDE_EXIT;
}
