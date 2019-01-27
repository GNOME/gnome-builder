/* gbp-git-branch.c
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

#define G_LOG_DOMAIN "gbp-git-branch"

#include "config.h"

#include <libide-vcs.h>

#include "gbp-git-branch.h"

struct _GbpGitBranch
{
  GObject parent_instance;
  gchar *name;
};

static gchar *
gbp_git_branch_get_name (IdeVcsBranch *branch)
{
  return g_strdup (GBP_GIT_BRANCH (branch)->name);
}

static void
vcs_branch_iface_init (IdeVcsBranchInterface *iface)
{
  iface->get_name = gbp_git_branch_get_name;
}

G_DEFINE_TYPE_WITH_CODE (GbpGitBranch, gbp_git_branch, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_BRANCH, vcs_branch_iface_init))

static void
gbp_git_branch_finalize (GObject *object)
{
  GbpGitBranch *self = (GbpGitBranch *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_git_branch_parent_class)->finalize (object);
}

static void
gbp_git_branch_class_init (GbpGitBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_branch_finalize;
}

static void
gbp_git_branch_init (GbpGitBranch *self)
{
}

GbpGitBranch *
gbp_git_branch_new (const gchar *name)
{
  GbpGitBranch *self;

  self = g_object_new (GBP_TYPE_GIT_BRANCH, NULL);
  self->name = g_strdup (name);

  return g_steal_pointer (&self);
}
