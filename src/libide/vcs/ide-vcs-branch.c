/* ide-vcs-branch.c
 *
 * Copyright 2019-2022 Christian Hergert <chergert@redhat.com>
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
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("id", NULL, NULL, NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("name", NULL, NULL, NULL,
                                                            (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

}

/**
 * ide_vcs_branch_dup_name:
 * @self: an #IdeVcsBranch
 *
 * Gets the name of the branch, which is used in various UI elements
 * to display to the user.
 *
 * Returns: (transfer full): a string containing the branch name
 */
char *
ide_vcs_branch_dup_name (IdeVcsBranch *self)
{
  char *name = NULL;
  g_return_val_if_fail (IDE_IS_VCS_BRANCH (self), NULL);
  g_object_get (self, "name", &name, NULL);
  return name;
}

/**
 * ide_vcs_branch_dup_id:
 * @self: an #IdeVcsBranch
 *
 * Gets the identifier of the branch.
 *
 * Returns: (transfer full): a string containing the branch identifier
 */
char *
ide_vcs_branch_dup_id (IdeVcsBranch *self)
{
  char *id = NULL;
  g_return_val_if_fail (IDE_IS_VCS_BRANCH (self), NULL);
  g_object_get (self, "id", &id, NULL);
  return id;
}
