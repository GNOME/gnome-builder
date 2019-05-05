/* ide-vcs-branch.h
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

#pragma once

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_BRANCH (ide_vcs_branch_get_type ())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeVcsBranch, ide_vcs_branch, IDE, VCS_BRANCH, GObject)

struct _IdeVcsBranchInterface
{
  GTypeInterface parent;

  gchar *(*get_name) (IdeVcsBranch *self);
  gchar *(*get_id)   (IdeVcsBranch *self);
};

IDE_AVAILABLE_IN_3_32
gchar *ide_vcs_branch_get_name (IdeVcsBranch *self);
IDE_AVAILABLE_IN_3_34
gchar *ide_vcs_branch_get_id   (IdeVcsBranch *self);

G_END_DECLS
