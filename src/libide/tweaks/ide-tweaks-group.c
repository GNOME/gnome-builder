/* ide-tweaks-group.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-group"

#include "config.h"

#include "ide-tweaks-group.h"
#include "ide-tweaks-widget.h"

struct _IdeTweaksGroup
{
  IdeTweaksItem parent_instance;
  char *title;
};

enum {
  PROP_0,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksGroup, ide_tweaks_group, IDE_TYPE_TWEAKS_ITEM)

static GParamSpec *properties [N_PROPS];

static gboolean
ide_tweaks_group_accepts (IdeTweaksItem *item,
                          IdeTweaksItem *child)
{
  g_assert (IDE_IS_TWEAKS_GROUP (item));
  g_assert (IDE_IS_TWEAKS_ITEM (child));

  return IDE_IS_TWEAKS_WIDGET (child);
}

static void
ide_tweaks_group_dispose (GObject *object)
{
  IdeTweaksGroup *self = (IdeTweaksGroup *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_tweaks_group_parent_class)->dispose (object);
}

static void
ide_tweaks_group_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksGroup *self = IDE_TWEAKS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_group_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_group_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksGroup *self = IDE_TWEAKS_GROUP (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      ide_tweaks_group_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_group_class_init (IdeTweaksGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_group_dispose;
  object_class->get_property = ide_tweaks_group_get_property;
  object_class->set_property = ide_tweaks_group_set_property;

  item_class->accepts = ide_tweaks_group_accepts;

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_group_init (IdeTweaksGroup *self)
{
}

IdeTweaksGroup *
ide_tweaks_group_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_GROUP, NULL);
}

const char *
ide_tweaks_group_get_title (IdeTweaksGroup *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_GROUP (self), NULL);

  return self->title;
}

void
ide_tweaks_group_set_title (IdeTweaksGroup *self,
                            const char     *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_GROUP (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
