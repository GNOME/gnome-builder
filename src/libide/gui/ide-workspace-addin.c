/* ide-workspace-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workspace-addin"

#include "config.h"

#include "ide-workspace.h"
#include "ide-workspace-addin.h"

/**
 * SECTION:ide-workspace-addin
 * @title: IdeWorkspaceAddin
 * @short_description: Extend the #IdeWorkspace windows
 *
 * The #IdeWorkspaceAddin is created with each #IdeWorkspace, allowing
 * plugins a chance to modify each window that is created.
 *
 * If you set `X-Workspace-Kind=primary` in your `.plugin` file, your
 * addin will only be loaded in the primary workspace. You may specify
 * multiple workspace kinds such as `primary` or `secondary` separated
 * by a comma such as `primary,secondary;`.
 */

G_DEFINE_INTERFACE (IdeWorkspaceAddin, ide_workspace_addin, G_TYPE_OBJECT)

static void
ide_workspace_addin_default_init (IdeWorkspaceAddinInterface *iface)
{
}

/**
 * ide_workspace_addin_load:
 * @self: a #IdeWorkspaceAddin
 *
 * Lods the #IdeWorkspaceAddin.
 *
 * This is a good place to modify the workspace from your addin.
 * Remember to unmodify the workspace in ide_workspace_addin_unload().
 */
void
ide_workspace_addin_load (IdeWorkspaceAddin *self,
                          IdeWorkspace      *workspace)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKSPACE_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->load)
    IDE_WORKSPACE_ADDIN_GET_IFACE (self)->load (self, workspace);
}

/**
 * ide_workspace_addin_unload:
 * @self: a #IdeWorkspaceAddin
 *
 * Unloads the #IdeWorkspaceAddin.
 *
 * This is a good place to unmodify the workspace from anything you
 * did in ide_workspace_addin_load().
 */
void
ide_workspace_addin_unload (IdeWorkspaceAddin *self,
                            IdeWorkspace      *workspace)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKSPACE_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->unload)
    IDE_WORKSPACE_ADDIN_GET_IFACE (self)->unload (self, workspace);
}

/**
 * ide_workspace_addin_page_changed:
 * @self: a #IdeWorkspaceAddin
 * @page: (nullable): an #IdePage or %NULL
 *
 * Called when the current page has changed based on focus within
 * the workspace.
 */
void
ide_workspace_addin_page_changed (IdeWorkspaceAddin *self,
                                  IdePage           *page)
{
  g_return_if_fail (IDE_IS_WORKSPACE_ADDIN (self));
  g_return_if_fail (!page || IDE_IS_PAGE (page));

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->page_changed)
    IDE_WORKSPACE_ADDIN_GET_IFACE (self)->page_changed (self, page);
}

/**
 * ide_workspace_addin_ref_action_group:
 * @self: a #IdeWorkspaceAddin
 *
 * Gets the action group for the workspace addin. This is automatically
 * registered with an action prefix like "workspace.module-name" where
 * "module-name" is the value of "Module=" in the plugin's manifest.
 *
 * Returns: (transfer full) (nullable): a #GActionGroup or %NULL
 */
GActionGroup *
ide_workspace_addin_ref_action_group (IdeWorkspaceAddin *self)
{
  GActionGroup *action_group = NULL;

  g_return_val_if_fail (IDE_IS_WORKSPACE_ADDIN (self), NULL);

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->ref_action_group)
    action_group = IDE_WORKSPACE_ADDIN_GET_IFACE (self)->ref_action_group (self);

  if (action_group == NULL && G_IS_ACTION_GROUP (self))
    action_group = g_object_ref (G_ACTION_GROUP (self));

  return action_group;
}

void
ide_workspace_addin_save_session (IdeWorkspaceAddin *self,
                                  IdeSession        *session)
{
  g_return_if_fail (IDE_IS_WORKSPACE_ADDIN (self));
  g_return_if_fail (IDE_IS_SESSION (session));

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->save_session)
    IDE_WORKSPACE_ADDIN_GET_IFACE (self)->save_session (self, session);
}
