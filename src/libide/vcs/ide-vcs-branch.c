/* ide-vcs-branch.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-vcs-branch"

#include "config.h"

#include "ide-vcs-branch.h"

G_DEFINE_INTERFACE (IdeVcsBranch, ide_vcs_branch, G_TYPE_OBJECT)

static void
ide_vcs_branch_default_init (IdeVcsBranchInterface *iface)
{
}

/**
 * ide_vcs_branch_get_name:
 * @self: an #IdeVcsBranch
 *
 * Gets the name of the branch, which is used in various UI elements
 * to display to the user.
 *
 * Returns: (transfer full): a string containing the branch name
 *
 * Since: 3.32
 */
gchar *
ide_vcs_branch_get_name (IdeVcsBranch *self)
{
  g_return_val_if_fail (IDE_IS_VCS_BRANCH (self), NULL);

  if (IDE_VCS_BRANCH_GET_IFACE (self)->get_name)
    return IDE_VCS_BRANCH_GET_IFACE (self)->get_name (self);

  return NULL;
}

/**
 * ide_vcs_branch_get_id:
 * @self: an #IdeVcsBranch
 *
 * Gets the identifier of the branch.
 *
 * Returns: (transfer full): a string containing the branch identifier
 *
 * Since: 3.34
 */
gchar *
ide_vcs_branch_get_id (IdeVcsBranch *self)
{
  g_return_val_if_fail (IDE_IS_VCS_BRANCH (self), NULL);

  if (IDE_VCS_BRANCH_GET_IFACE (self)->get_id)
    return IDE_VCS_BRANCH_GET_IFACE (self)->get_id (self);

  return NULL;
}
