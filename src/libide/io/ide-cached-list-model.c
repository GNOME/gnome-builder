/* ide-cached-list-model.c
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

#define G_LOG_DOMAIN "ide-cached-list-model"

#include "config.h"

#include "ide-cached-list-model.h"

struct _IdeCachedListModel
{
  GObject     parent_instance;
  GSequence  *items;
  GListModel *model;
  gulong      items_changed_handler;

  guint       in_dispose : 1;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GType
ide_cached_list_model_get_item_type (GListModel *model)
{
  IdeCachedListModel *self = IDE_CACHED_LIST_MODEL (model);

  if (self->model == NULL)
    return G_TYPE_OBJECT;

  return g_list_model_get_item_type (self->model);
}

static guint
ide_cached_list_model_get_n_items (GListModel *model)
{
  IdeCachedListModel *self = IDE_CACHED_LIST_MODEL (model);

  if (self->model == NULL)
    return 0;

  return g_list_model_get_n_items (self->model);
}

static gpointer
ide_cached_list_model_get_item (GListModel *model,
                                guint       position)
{
  IdeCachedListModel *self = IDE_CACHED_LIST_MODEL (model);
  GSequenceIter *iter;
  gpointer item;

  if (self->model == NULL)
    return NULL;

  iter = g_sequence_get_iter_at_pos (self->items, position);
  if (g_sequence_iter_is_end (iter))
    return NULL;

  if (!(item = g_sequence_get (iter)))
    {
      item = g_list_model_get_item (self->model, position);
      g_sequence_set (iter, item);
    }

  return g_object_ref (item);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_cached_list_model_get_item_type;
  iface->get_n_items = ide_cached_list_model_get_n_items;
  iface->get_item = ide_cached_list_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCachedListModel, ide_cached_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
_g_object_xunref (gpointer data)
{
  if (data)
    g_object_unref (data);
}

static void
ide_cached_list_model_dispose (GObject *object)
{
  IdeCachedListModel *self = (IdeCachedListModel *)object;

  self->in_dispose = TRUE;

  ide_cached_list_model_set_model (self, NULL);

  g_assert (self->model == NULL);
  g_assert (self->items == NULL);
  g_assert (self->items_changed_handler == 0);

  G_OBJECT_CLASS (ide_cached_list_model_parent_class)->dispose (object);
}

static void
ide_cached_list_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeCachedListModel *self = IDE_CACHED_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, ide_cached_list_model_get_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cached_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeCachedListModel *self = IDE_CACHED_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      ide_cached_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cached_list_model_class_init (IdeCachedListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_cached_list_model_dispose;
  object_class->get_property = ide_cached_list_model_get_property;
  object_class->set_property = ide_cached_list_model_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_cached_list_model_init (IdeCachedListModel *self)
{
}

/**
 * ide_cached_list_model_get_model:
 * @self: a #IdeCachedListModel
 *
 * Gets the underlying model.
 *
 * Returns: (transfer none) (nullable): a #GListModel or %NULL
 */
GListModel *
ide_cached_list_model_get_model (IdeCachedListModel *self)
{
  g_return_val_if_fail (IDE_IS_CACHED_LIST_MODEL (self), NULL);

  return self->model;
}

static void
ide_cached_list_model_items_changed_cb (IdeCachedListModel *self,
                                        guint               position,
                                        guint               removed,
                                        guint               added,
                                        GListModel         *model)
{
  GSequenceIter *iter;

  g_assert (IDE_IS_CACHED_LIST_MODEL (self));
  g_assert (G_IS_LIST_MODEL (model));
  g_assert (self->items != NULL);

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (removed > 0)
    {
      GSequenceIter *end;

      if (removed == 1)
        end = iter;
      else
        end = g_sequence_iter_prev (g_sequence_get_iter_at_pos (self->items, position+removed));

      g_sequence_remove_range (iter, end);

      iter = g_sequence_get_iter_at_pos (self->items, position);
    }

  for (guint i = 0; i < added; i++)
    g_sequence_insert_before (iter, NULL);

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

void
ide_cached_list_model_set_model (IdeCachedListModel *self,
                                 GListModel         *model)
{
  g_autoptr(GSequence) old_items = NULL;
  guint removed = 0;
  guint added = 0;

  g_return_if_fail (IDE_IS_CACHED_LIST_MODEL (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  if (self->model != NULL)
    {
      removed = g_list_model_get_n_items (self->model);
      g_clear_signal_handler (&self->items_changed_handler, self->model);
      old_items = g_steal_pointer (&self->items);
    }

  if (model != NULL)
    {
      GSequenceIter *end;

      added = g_list_model_get_n_items (model);
      self->items_changed_handler =
        g_signal_connect_object (model,
                                 "items-changed",
                                 G_CALLBACK (ide_cached_list_model_items_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

      self->items = g_sequence_new (_g_object_xunref);
      end = g_sequence_get_end_iter (self->items);
      for (guint i = 0; i < added; i++)
        g_sequence_insert_before (end, NULL);
    }

  g_set_object (&self->model, model);

  if (!self->in_dispose)
    {
      if (removed || added)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * ide_cached_list_model_new:
 * @model: (nullable) (transfer full): a #GListModel or %NULL
 *
 * Creates a new list model that caches the items.
 *
 * This might be useful if have a GtkMapListModel which would otherwise discard your
 * mapped items.
 *
 * Returns: (transfer full): an #IdeCachedListModel
 */
IdeCachedListModel *
ide_cached_list_model_new (GListModel *model)
{
  IdeCachedListModel *ret;

  g_return_val_if_fail (!model || G_IS_LIST_MODEL (model), NULL);

  ret = g_object_new (IDE_TYPE_CACHED_LIST_MODEL,
                      "model", model,
                      NULL);
  g_clear_object (&model);
  return ret;
}
