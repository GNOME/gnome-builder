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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
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
 * ide_workspace_addin_surface_set:
 * @self: an #IdeWorkspaceAddin
 * @surface: (nullable): an #IdeSurface or %NULL
 *
 * This function is called to notify the addin of the current surface.
 * It may be set to %NULL before unloading the addin to allow addins
 * to do surface change state handling and cleanup in one function.
 *
 * Since: 3.32
 */
void
ide_workspace_addin_surface_set (IdeWorkspaceAddin *self,
                                 IdeSurface        *surface)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKSPACE_ADDIN (self));
  g_return_if_fail (!surface || IDE_IS_SURFACE (surface));

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->surface_set)
    IDE_WORKSPACE_ADDIN_GET_IFACE (self)->surface_set (self, surface);
}

/**
 * ide_workspace_addin_can_close:
 * @self: an #IdeWorkspaceAddin
 *
 * This method is called to determine if the workspace can close. If the addin
 * needs to prevent the workspace closing, then return %FALSE; otherwise %TRUE.
 *
 * Returns: %TRUE if the workspace can close; otherwise %FALSE.
 *
 * Since: 3.34
 */
gboolean
ide_workspace_addin_can_close (IdeWorkspaceAddin *self)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE_ADDIN (self), TRUE);

  if (IDE_WORKSPACE_ADDIN_GET_IFACE (self)->can_close)
    return IDE_WORKSPACE_ADDIN_GET_IFACE (self)->can_close (self);

  return TRUE;
}
