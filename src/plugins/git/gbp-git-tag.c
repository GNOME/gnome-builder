/* gbp-git-tag.c
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

#define G_LOG_DOMAIN "gbp-git-tag"

#include "config.h"

#include <libide-vcs.h>

#include "gbp-git-tag.h"

struct _GbpGitTag
{
  GObject parent_instance;
  char *name;
};

static char *
gbp_git_tag_dup_name (IdeVcsTag *tag)
{
  return g_strdup (GBP_GIT_TAG (tag)->name);
}

static void
vcs_tag_iface_init (IdeVcsTagInterface *iface)
{
  iface->dup_name = gbp_git_tag_dup_name;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitTag, gbp_git_tag, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS_TAG, vcs_tag_iface_init))

enum {
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gbp_git_tag_finalize (GObject *object)
{
  GbpGitTag *self = (GbpGitTag *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gbp_git_tag_parent_class)->finalize (object);
}

static void
gbp_git_tag_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbpGitTag *self = GBP_GIT_TAG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_tag_class_init (GbpGitTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_tag_finalize;
  object_class->get_property = gbp_git_tag_get_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_tag_init (GbpGitTag *self)
{
}

GbpGitTag *
gbp_git_tag_new (const gchar *name)
{
  GbpGitTag *self;

  self = g_object_new (GBP_TYPE_GIT_TAG, NULL);
  self->name = g_strdup (name);

  return g_steal_pointer (&self);
}
