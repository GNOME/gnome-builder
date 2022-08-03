/* ide-tweaks-factory.c
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

#define G_LOG_DOMAIN "ide-tweaks-factory"

#include "config.h"

#include "ide-tweaks-factory-private.h"
#include "ide-tweaks-item-private.h"

struct _IdeTweaksFactory
{
  IdeTweaksItem  parent_instance;
  GListModel    *model;
  GObject       *item;
};

G_DEFINE_FINAL_TYPE (IdeTweaksFactory, ide_tweaks_factory, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_ITEM,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
ide_tweaks_factory_accepts (IdeTweaksItem *item,
                            IdeTweaksItem *child)
{
  return TRUE;
}

static void
ide_tweaks_factory_dispose (GObject *object)
{
  IdeTweaksFactory *self = (IdeTweaksFactory *)object;

  g_clear_object (&self->model);
  g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_tweaks_factory_parent_class)->dispose (object);
}

static void
ide_tweaks_factory_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTweaksFactory *self = IDE_TWEAKS_FACTORY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, ide_tweaks_factory_get_model (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_factory_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTweaksFactory *self = IDE_TWEAKS_FACTORY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ide_tweaks_factory_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_factory_class_init (IdeTweaksFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_factory_dispose;
  object_class->get_property = ide_tweaks_factory_get_property;
  object_class->set_property = ide_tweaks_factory_set_property;

  item_class->accepts = ide_tweaks_factory_accepts;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_factory_init (IdeTweaksFactory *self)
{
}

/**
 * ide_tweaks_factory_get_model:
 * @self: a #IdeTweaksFactory
 *
 * Returns: (transfer none) (nullable): a #GListModel or %NULL
 */
GListModel *
ide_tweaks_factory_get_model (IdeTweaksFactory *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_FACTORY (self), NULL);

  return self->model;
}

void
ide_tweaks_factory_set_model (IdeTweaksFactory *self,
                              GListModel       *model)
{
  g_return_if_fail (IDE_IS_TWEAKS_FACTORY (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));

  if (g_set_object (&self->model, model))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
}

gboolean
_ide_tweaks_factory_is_one_of (IdeTweaksFactory *self,
                               const GType      *allowed_types,
                               guint             n_allowed_types)
{
  IdeTweaksItem *child;
  GType child_type;

  g_return_val_if_fail (IDE_IS_TWEAKS_FACTORY (self), FALSE);
  g_return_val_if_fail (allowed_types || n_allowed_types == 0, FALSE);

  if (!(child = ide_tweaks_item_get_first_child (IDE_TWEAKS_ITEM (self))))
    return FALSE;

  child_type = G_OBJECT_TYPE (child);

  for (guint i = 0; i < n_allowed_types; i++)
    {
      if (g_type_is_a (child_type, allowed_types[i]))
        return TRUE;
    }

  return FALSE;
}

GPtrArray *
_ide_tweaks_factory_inflate (IdeTweaksFactory *self)
{
  g_autoptr(GListModel) model = NULL;
  IdeTweaksItem *child;
  GPtrArray *ar;
  guint n_items;

  g_return_val_if_fail (IDE_IS_TWEAKS_FACTORY (self), NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  if (!g_set_object (&model, self->model))
    return ar;

  if (!(child = ide_tweaks_item_get_first_child (IDE_TWEAKS_ITEM (self))))
    return ar;

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) object = g_list_model_get_item (model, i);

      /* Allow bindings on/descendant-to @child to update */
      if (g_set_object (&self->item, object))
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);

      /* Now deep copy child to snapshot state */
      g_ptr_array_add (ar, ide_tweaks_item_copy (child));
    }

  if (g_set_object (&self->item, NULL))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);

  return ar;
}
