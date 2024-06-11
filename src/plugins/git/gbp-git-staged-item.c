/*
 * gbp-git-staged-item.c
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

#include "gbp-git-staged-item.h"

struct _GbpGitStagedItem
{
  GbpGitCommitItem parent_instance;
  GFile *file;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpGitStagedItem, gbp_git_staged_item, GBP_TYPE_GIT_COMMIT_ITEM)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_staged_item_dispose (GObject *object)
{
  GbpGitStagedItem *self = (GbpGitStagedItem *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gbp_git_staged_item_parent_class)->dispose (object);
}

static void
gbp_git_staged_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpGitStagedItem *self = GBP_GIT_STAGED_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gbp_git_staged_item_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpGitStagedItem *self = GBP_GIT_STAGED_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gbp_git_staged_item_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_staged_item_class_init (GbpGitStagedItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_git_staged_item_dispose;
  object_class->get_property = gbp_git_staged_item_get_property;
  object_class->set_property = gbp_git_staged_item_set_property;

  properties[PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_staged_item_init (GbpGitStagedItem *self)
{
}

GFile *
gbp_git_staged_item_get_file (GbpGitStagedItem *self)
{
  g_return_val_if_fail (GBP_IS_GIT_STAGED_ITEM (self), NULL);

  return self->file;
}

void
gbp_git_staged_item_set_file (GbpGitStagedItem *self,
                              GFile            *file)
{
  g_return_if_fail (GBP_IS_GIT_STAGED_ITEM (self));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
}
