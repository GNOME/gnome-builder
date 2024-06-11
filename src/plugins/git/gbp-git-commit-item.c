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

typedef struct
{
  const char *icon_name;
  char *title;
} GbpGitCommitItemPrivate;

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_SECTION_TITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GbpGitCommitItem, gbp_git_commit_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gbp_git_commit_item_dispose (GObject *object)
{
  GbpGitCommitItem *self = (GbpGitCommitItem *)object;
  GbpGitCommitItemPrivate *priv = gbp_git_commit_item_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (gbp_git_commit_item_parent_class)->dispose (object);
}

static void
gbp_git_commit_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpGitCommitItem *self = GBP_GIT_COMMIT_ITEM (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, gbp_git_commit_item_get_icon_name (self));
      break;

    case PROP_SECTION_TITLE:
      g_value_set_string (value, GBP_GIT_COMMIT_ITEM_GET_CLASS (self)->get_section_title (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gbp_git_commit_item_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpGitCommitItem *self = GBP_GIT_COMMIT_ITEM (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gbp_git_commit_item_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gbp_git_commit_item_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_commit_item_class_init (GbpGitCommitItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_git_commit_item_dispose;
  object_class->get_property = gbp_git_commit_item_get_property;
  object_class->set_property = gbp_git_commit_item_set_property;

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_SECTION_TITLE] =
    g_param_spec_string ("section-title", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_commit_item_init (GbpGitCommitItem *self)
{
}

const char *
gbp_git_commit_item_get_title (GbpGitCommitItem *self)
{
  GbpGitCommitItemPrivate *priv = gbp_git_commit_item_get_instance_private (self);

  g_return_val_if_fail (GBP_IS_GIT_COMMIT_ITEM (self), NULL);

  return priv->title;
}

void
gbp_git_commit_item_set_title (GbpGitCommitItem *self,
                               const char       *title)
{
  GbpGitCommitItemPrivate *priv = gbp_git_commit_item_get_instance_private (self);

  g_return_if_fail (GBP_IS_GIT_COMMIT_ITEM (self));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
gbp_git_commit_item_get_icon_name (GbpGitCommitItem *self)
{
  GbpGitCommitItemPrivate *priv = gbp_git_commit_item_get_instance_private (self);

  g_return_val_if_fail (GBP_IS_GIT_COMMIT_ITEM (self), NULL);

  return priv->icon_name;
}

void
gbp_git_commit_item_set_icon_name (GbpGitCommitItem *self,
                                   const char       *icon_name)
{
  GbpGitCommitItemPrivate *priv = gbp_git_commit_item_get_instance_private (self);

  g_return_if_fail (GBP_IS_GIT_COMMIT_ITEM (self));

  icon_name = g_intern_string (icon_name);

  if (icon_name != priv->icon_name)
    {
      priv->icon_name = icon_name;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
    }
}

GtkWidget *
gbp_git_commit_item_create_row (GbpGitCommitItem *self)
{
  g_return_val_if_fail (GBP_IS_GIT_COMMIT_ITEM (self), NULL);

  return GBP_GIT_COMMIT_ITEM_GET_CLASS (self)->create_row (self);
}
