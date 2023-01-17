/* ide-search-popover-group.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-search-popover-group"

#include "ide-search-popover-group-private.h"

struct _IdeSearchPopoverGroup
{
  GObject parent_instance;
  char *icon_name;
  char *title;
  IdeSearchCategory category;
};

enum {
  PROP_0,
  PROP_CATEGORY,
  PROP_ICON_NAME,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeSearchPopoverGroup, ide_search_popover_group, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_search_popover_group_dispose (GObject *object)
{
  IdeSearchPopoverGroup *self = (IdeSearchPopoverGroup *)object;

  g_clear_pointer (&self->icon_name, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_search_popover_group_parent_class)->dispose (object);
}

static void
ide_search_popover_group_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeSearchPopoverGroup *self = IDE_SEARCH_POPOVER_GROUP (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      g_value_set_enum (value, self->category);
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, self->icon_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_group_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeSearchPopoverGroup *self = IDE_SEARCH_POPOVER_GROUP (object);

  switch (prop_id)
    {
    case PROP_CATEGORY:
      self->category = g_value_get_enum (value);
      break;

    case PROP_ICON_NAME:
      self->icon_name = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_group_class_init (IdeSearchPopoverGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_search_popover_group_dispose;
  object_class->get_property = ide_search_popover_group_get_property;
  object_class->set_property = ide_search_popover_group_set_property;

  properties[PROP_CATEGORY] =
    g_param_spec_enum ("category", NULL, NULL,
                       IDE_TYPE_SEARCH_CATEGORY,
                       IDE_SEARCH_CATEGORY_OTHER,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_search_popover_group_init (IdeSearchPopoverGroup *self)
{
  self->category = IDE_SEARCH_CATEGORY_OTHER;
}

const char *
ide_search_popover_group_get_title (IdeSearchPopoverGroup *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_POPOVER_GROUP (self), NULL);

  return self->title;
}

const char *
ide_search_popover_group_get_icon_name (IdeSearchPopoverGroup *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_POPOVER_GROUP (self), NULL);

  return self->icon_name;
}

IdeSearchCategory
ide_search_popover_group_get_category (IdeSearchPopoverGroup *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_POPOVER_GROUP (self), 0);

  return self->category;
}
