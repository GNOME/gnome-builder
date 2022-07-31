/* ide-tweaks-subpage-factory.c
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

#define G_LOG_DOMAIN "ide-tweaks-subpage-factory"

#include "config.h"

#include "ide-tweaks-subpage.h"
#include "ide-tweaks-subpage-factory.h"

struct _IdeTweaksSubpageFactory
{
  IdeTweaksItem  parent_instance;
  GListModel    *model;
};

G_DEFINE_FINAL_TYPE (IdeTweaksSubpageFactory, ide_tweaks_subpage_factory, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
ide_tweaks_subpage_factory_accepts (IdeTweaksItem *item,
                                    IdeTweaksItem *child)
{
  return IDE_IS_TWEAKS_SUBPAGE (child);
}

static void
ide_tweaks_subpage_factory_dispose (GObject *object)
{
  IdeTweaksSubpageFactory *self = (IdeTweaksSubpageFactory *)object;

  g_clear_object (&self->model);

  G_OBJECT_CLASS (ide_tweaks_subpage_factory_parent_class)->dispose (object);
}

static void
ide_tweaks_subpage_factory_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdeTweaksSubpageFactory *self = IDE_TWEAKS_SUBPAGE_FACTORY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, ide_tweaks_subpage_factory_get_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_subpage_factory_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdeTweaksSubpageFactory *self = IDE_TWEAKS_SUBPAGE_FACTORY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ide_tweaks_subpage_factory_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_subpage_factory_class_init (IdeTweaksSubpageFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_subpage_factory_dispose;
  object_class->get_property = ide_tweaks_subpage_factory_get_property;
  object_class->set_property = ide_tweaks_subpage_factory_set_property;

  item_class->accepts = ide_tweaks_subpage_factory_accepts;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_subpage_factory_init (IdeTweaksSubpageFactory *self)
{
}

/**
 * ide_tweaks_subpage_factory_get_model:
 * @self: a #IdeTweaksSubpageFactory
 *
 * Returns: (transfer none) (nullable): a #GListModel or %NULL
 */
GListModel *
ide_tweaks_subpage_factory_get_model (IdeTweaksSubpageFactory *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SUBPAGE_FACTORY (self), NULL);

  return self->model;
}

void
ide_tweaks_subpage_factory_set_model (IdeTweaksSubpageFactory *self,
                                      GListModel              *model)
{
  g_return_if_fail (IDE_IS_TWEAKS_SUBPAGE_FACTORY (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));

  if (g_set_object (&self->model, model))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
}
