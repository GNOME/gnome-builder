/*
 * ide-tweaks-model.c
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

#define G_LOG_DOMAIN "ide-tweaks-model"

#include "config.h"

#include "ide-tweaks-factory-private.h"
#include "ide-tweaks-model-private.h"

struct _IdeTweaksModel
{
  GObject               parent_instance;
  IdeTweaksItem        *item;
  GPtrArray            *items;
  GPtrArray            *branches;
  IdeTweaksItemVisitor  visitor;
  gpointer              visitor_data;
  GDestroyNotify        visitor_data_destroy;
};

static GType
list_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_TWEAKS_ITEM;
}

static guint
list_model_get_n_items (GListModel *model)
{
  return IDE_TWEAKS_MODEL (model)->items->len;
}

static gpointer
list_model_get_item (GListModel *model,
                     guint       position)
{
  IdeTweaksModel *self = IDE_TWEAKS_MODEL (model);

  if (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items = list_model_get_n_items;
  iface->get_item = list_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeTweaksModel, ide_tweaks_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_ITEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static IdeTweaksItemVisitResult
ide_tweaks_model_populate_cb (IdeTweaksItem *item,
                              gpointer       user_data)
{
  IdeTweaksModel *self = user_data;
  IdeTweaksItemVisitResult res;

  if (IDE_IS_TWEAKS_FACTORY (item))
    {
      if (ide_tweaks_factory_visit (IDE_TWEAKS_FACTORY (item),
                                    ide_tweaks_model_populate_cb,
                                    self))
        return IDE_TWEAKS_ITEM_VISIT_STOP;
      return IDE_TWEAKS_ITEM_VISIT_CONTINUE;
    }

  res = self->visitor (item, self->visitor_data);

  switch (res)
    {
    case IDE_TWEAKS_ITEM_VISIT_ACCEPT_AND_CONTINUE:
      g_ptr_array_add (self->items, g_object_ref (item));

      /* We might need to keep the parents around up to our factory so
       * that they are not disposed after visiting.
       */
      for (IdeTweaksItem *iter = ide_tweaks_item_get_parent (item);
           iter != NULL && iter != self->item;
           iter = ide_tweaks_item_get_parent (iter))
        {
          guint pos;

          if (!g_ptr_array_find (self->branches, iter, &pos))
            g_ptr_array_add (self->branches, g_object_ref (iter));
        }

      return IDE_TWEAKS_ITEM_VISIT_CONTINUE;

    case IDE_TWEAKS_ITEM_VISIT_RECURSE:
    case IDE_TWEAKS_ITEM_VISIT_STOP:
    case IDE_TWEAKS_ITEM_VISIT_CONTINUE:
    default:
      return res;
    }

  g_assert_not_reached ();
}

static void
ide_tweaks_model_populate (IdeTweaksModel *self,
                           IdeTweaksItem  *item)
{
  g_assert (IDE_IS_TWEAKS_MODEL (self));
  g_assert (self->items != NULL);
  g_assert (self->visitor != NULL);

  ide_tweaks_item_visit_children (item, ide_tweaks_model_populate_cb, self);
}

IdeTweaksModel *
ide_tweaks_model_new (IdeTweaksItem        *item,
                      IdeTweaksItemVisitor  visitor,
                      gpointer              visitor_data,
                      GDestroyNotify        visitor_data_destroy)
{
  IdeTweaksModel *self;

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (item), NULL);
  g_return_val_if_fail (visitor != NULL, NULL);

  self = g_object_new (IDE_TYPE_TWEAKS_MODEL, NULL);
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->visitor = visitor;
  self->visitor_data = visitor_data;
  self->visitor_data_destroy = visitor_data_destroy;

  if (g_set_object (&self->item, item))
    ide_tweaks_model_populate (self, item);

  return self;
}

static void
ide_tweaks_model_dispose (GObject *object)
{
  IdeTweaksModel *self = (IdeTweaksModel *)object;

  g_clear_object (&self->item);
  g_clear_pointer (&self->branches, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  if (self->visitor_data_destroy)
    {
      GDestroyNotify notify = g_steal_pointer (&self->visitor_data_destroy);
      self->visitor = NULL;
      g_clear_pointer (&self->visitor_data, notify);
    }

  G_OBJECT_CLASS (ide_tweaks_model_parent_class)->dispose (object);
}

static void
ide_tweaks_model_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksModel *self = IDE_TWEAKS_MODEL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, ide_tweaks_model_get_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_model_class_init (IdeTweaksModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_model_dispose;
  object_class->get_property = ide_tweaks_model_get_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         IDE_TYPE_TWEAKS_ITEM,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_model_init (IdeTweaksModel *self)
{
  self->branches = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * ide_tweaks_model_get_item:
 * @self: an #IdeTweaksModel
 *
 * Gets the parent item for the model.
 *
 * Returns: (transfer none): an #IdeItem
 */
IdeTweaksItem *
ide_tweaks_model_get_item (IdeTweaksModel *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_MODEL (self), NULL);

  return self->item;
}
