/*
 * gbp-git-commit-item.c
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

#include "gbp-git-commit-item.h"

enum {
  PROP_0,
  PROP_SECTION_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (GbpGitCommitItem, gbp_git_commit_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_commit_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpGitCommitItem *self = GBP_GIT_COMMIT_ITEM (object);

  switch (prop_id)
    {
    case PROP_SECTION_TITLE:
      g_value_set_string (value, GBP_GIT_COMMIT_ITEM_GET_CLASS (self)->get_section_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_item_class_init (GbpGitCommitItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_git_commit_item_get_property;

  properties[PROP_SECTION_TITLE] =
    g_param_spec_string ("section-title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_commit_item_init (GbpGitCommitItem *self)
{
}
