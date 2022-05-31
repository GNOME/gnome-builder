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
#include <string.h>

#include "gbp-git-branch.h"

struct _GbpGitBranch
{
  GObject parent_instance;
  char *id;
};

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitBranch, gbp_git_branch, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_BRANCH, NULL))

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static const char *
gbp_git_branch_get_name (GbpGitBranch *self)
{
  const char *id;

  g_assert (GBP_IS_GIT_BRANCH (self));

  if ((id = self->id) && g_str_has_prefix (id, "refs/heads/"))
    id += strlen ("refs/heads/");

  return id;
}

static void
gbp_git_branch_dispose (GObject *object)
{
  GbpGitBranch *self = (GbpGitBranch *)object;

  ide_clear_string (&self->id);

  G_OBJECT_CLASS (gbp_git_branch_parent_class)->dispose (object);
}

static void
gbp_git_branch_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpGitBranch *self = GBP_GIT_BRANCH (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, gbp_git_branch_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_branch_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpGitBranch *self = GBP_GIT_BRANCH (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_branch_class_init (GbpGitBranchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_git_branch_dispose;
  object_class->get_property = gbp_git_branch_get_property;
  object_class->set_property = gbp_git_branch_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_branch_init (GbpGitBranch *self)
{
}

GbpGitBranch *
gbp_git_branch_new (const char *id)
{
  return g_object_new (GBP_TYPE_GIT_BRANCH,
                       "id", id,
                       NULL);
}
