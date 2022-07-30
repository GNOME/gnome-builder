/* ide-tweaks-item.c
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

#define G_LOG_DOMAIN "ide-tweaks-item"

#include "config.h"

#include "ide-tweaks-item.h"

typedef struct
{
  char **keywords;
} IdeTweaksItemPrivate;

enum {
  PROP_0,
  PROP_KEYWORDS,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (IdeTweaksItem, ide_tweaks_item, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_item_finalize (GObject *object)
{
  IdeTweaksItem *self = (IdeTweaksItem *)object;
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_clear_pointer (&priv->keywords, g_strfreev);

  G_OBJECT_CLASS (ide_tweaks_item_parent_class)->finalize (object);
}

static void
ide_tweaks_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksItem *self = IDE_TWEAKS_ITEM (object);

  switch (prop_id)
    {
    case PROP_KEYWORDS:
      g_value_set_boxed (value, ide_tweaks_item_get_keywords (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksItem *self = IDE_TWEAKS_ITEM (object);

  switch (prop_id)
    {
    case PROP_KEYWORDS:
      ide_tweaks_item_set_keywords (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_item_class_init (IdeTweaksItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_tweaks_item_finalize;
  object_class->get_property = ide_tweaks_item_get_property;
  object_class->set_property = ide_tweaks_item_set_property;

  properties [PROP_KEYWORDS] =
    g_param_spec_boxed ("keywords", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_item_init (IdeTweaksItem *self)
{
}

const char * const *
ide_tweaks_item_get_keywords (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return (const char * const *)priv->keywords;
}

void
ide_tweaks_item_set_keywords (IdeTweaksItem      *self,
                              const char * const *keywords)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));

  if (keywords == priv->keywords)
    return;

  g_strfreev (priv->keywords);
  priv->keywords = g_strdupv ((char **)keywords);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEYWORDS]);
}
