/*
 * gbp-git-workspace-addin.c
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

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-git-commit-dialog.h"
#include "gbp-git-workspace-addin.h"

struct _GbpGitWorkspaceAddin
{
  GObject             parent_instance;

  IdeWorkspace       *workspace;
  GbpGitCommitDialog *commit_dialog;

  GtkMenuButton      *button;
};

static void begin_commit_action (GbpGitWorkspaceAddin *self,
                                 GVariant             *param);

IDE_DEFINE_ACTION_GROUP (GbpGitWorkspaceAddin, gbp_git_workspace_addin, {
  { "begin-commit", begin_commit_action },
})

static void
gbp_git_workspace_addin_load (IdeWorkspaceAddin *addin,
                              IdeWorkspace      *workspace)
{
  GbpGitWorkspaceAddin *self = GBP_GIT_WORKSPACE_ADDIN (addin);
  IdeHeaderBar *header_bar;
  GMenu *menu;

  g_assert (GBP_IS_GIT_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) || IDE_IS_EDITOR_WORKSPACE (workspace));

  self->workspace = workspace;

  header_bar = ide_workspace_get_header_bar (workspace);
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "gbp-git-menu");

  self->button = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "always-show-arrow", TRUE,
                               "icon-name", "builder-vcs-git-symbolic",
                               "menu-model", menu,
                               NULL);

  ide_header_bar_add (header_bar,
                      IDE_HEADER_BAR_POSITION_LEFT_OF_CENTER,
                      0,
                      GTK_WIDGET (self->button));
}

static void
gbp_git_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                IdeWorkspace      *workspace)
{
  GbpGitWorkspaceAddin *self = GBP_GIT_WORKSPACE_ADDIN (addin);
  IdeHeaderBar *header_bar;

  g_assert (GBP_IS_GIT_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) || IDE_IS_EDITOR_WORKSPACE (workspace));

  g_clear_weak_pointer (&self->commit_dialog);

  header_bar = ide_workspace_get_header_bar (workspace);
  ide_header_bar_remove (header_bar, GTK_WIDGET (self->button));

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_git_workspace_addin_load;
  iface->unload = gbp_git_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitWorkspaceAddin, gbp_git_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_git_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_git_workspace_addin_class_init (GbpGitWorkspaceAddinClass *klass)
{
}

static void
gbp_git_workspace_addin_init (GbpGitWorkspaceAddin *self)
{
}

static void
begin_commit_action (GbpGitWorkspaceAddin *self,
                     GVariant             *param)
{
  GbpGitCommitDialog *dialog;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  if (self->commit_dialog != NULL)
    IDE_EXIT;

  context = ide_workspace_get_context (self->workspace);
  dialog = gbp_git_commit_dialog_new (context);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self->workspace));

  g_set_weak_pointer (&self->commit_dialog, dialog);

  IDE_EXIT;
}
