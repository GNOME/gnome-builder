/*
 * manuals-search-model.c
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

#include "manuals-gom.h"
#include "manuals-navigatable.h"
#include "manuals-search-model.h"
#include "manuals-search-result.h"

#define PER_FETCH_GROUP 100

struct _ManualsSearchModel
{
  GObject           parent_instance;
  GomResourceGroup *group;
  GPtrArray        *prefetch;
  GHashTable       *items;
};

static void
_dex_xunref (gpointer instance)
{
  if (instance)
    dex_unref (instance);
}

static GType
manuals_search_model_get_item_type (GListModel *model)
{
  return MANUALS_TYPE_SEARCH_RESULT;
}

static guint
manuals_search_model_get_n_items (GListModel *model)
{
  ManualsSearchModel *self = MANUALS_SEARCH_MODEL (model);

  if (self->group != NULL)
    return gom_resource_group_get_count (self->group);

  return 0;
}

static DexFuture *
manuals_search_model_fetch_item_cb (DexFuture *completed,
                                    gpointer   user_data)
{
  g_autoptr(GomResourceGroup) group = NULL;
  ManualsSearchResult *result = user_data;
  GomResource *resource;
  guint position;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (MANUALS_IS_SEARCH_RESULT (result));

  position = manuals_search_result_get_position (result);

  group = dex_await_object (dex_ref (completed), NULL);
  g_assert (GOM_IS_RESOURCE_GROUP (group));
  g_assert (position < gom_resource_group_get_count (group));

  resource = gom_resource_group_get_index (group, position);
  g_assert (!resource || GOM_IS_RESOURCE (resource));

  if (resource != NULL)
    {
      g_autoptr(ManualsNavigatable) navigatable = NULL;

      navigatable = manuals_navigatable_new_for_resource (G_OBJECT (resource));
      manuals_search_result_set_item (result, navigatable);
    }

  return dex_future_new_for_boolean (TRUE);
}

static gpointer
manuals_search_model_get_item (GListModel *model,
                               guint       position)
{
  ManualsSearchModel *self = MANUALS_SEARCH_MODEL (model);
  ManualsSearchResult *result;
  DexFuture *fetch;
  guint fetch_index;

  if (self->group == NULL)
    return NULL;

  if (position >= gom_resource_group_get_count (self->group))
    return NULL;

  /* If we already got this item before, give the same pointer again */
  if ((result = g_hash_table_lookup (self->items, GUINT_TO_POINTER (position))))
    return g_object_ref (result);

  fetch_index = position / PER_FETCH_GROUP;
  if (fetch_index >= self->prefetch->len)
    g_ptr_array_set_size (self->prefetch, fetch_index+1);

  if (!(fetch = g_ptr_array_index (self->prefetch, fetch_index)))
    {
      fetch = gom_resource_group_fetch (self->group,
                                        fetch_index * PER_FETCH_GROUP,
                                        PER_FETCH_GROUP);
      g_ptr_array_index (self->prefetch, fetch_index) = fetch;
    }

  result = manuals_search_result_new (position);

  /* Make sure we have a stable item across get calls */
  g_hash_table_insert (self->items,
                       GUINT_TO_POINTER (position),
                       g_object_ref (result));

  dex_future_disown (dex_future_then (dex_ref (fetch),
                                      manuals_search_model_fetch_item_cb,
                                      g_object_ref (result),
                                      g_object_unref));

  return result;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = manuals_search_model_get_n_items;
  iface->get_item_type = manuals_search_model_get_item_type;
  iface->get_item = manuals_search_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsSearchModel, manuals_search_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_GROUP,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

ManualsSearchModel *
manuals_search_model_new (GomResourceGroup *group)
{
  g_return_val_if_fail (GOM_IS_RESOURCE_GROUP (group), NULL);

  return g_object_new (MANUALS_TYPE_SEARCH_MODEL,
                       "group", group,
                       NULL);
}

static void
manuals_search_model_dispose (GObject *object)
{
  ManualsSearchModel *self = (ManualsSearchModel *)object;

  g_clear_pointer (&self->prefetch, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_hash_table_unref);
  g_clear_object (&self->group);

  G_OBJECT_CLASS (manuals_search_model_parent_class)->dispose (object);
}

static void
manuals_search_model_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ManualsSearchModel *self = MANUALS_SEARCH_MODEL (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_object (value, self->group);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_model_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ManualsSearchModel *self = MANUALS_SEARCH_MODEL (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      self->group = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_search_model_class_init (ManualsSearchModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = manuals_search_model_dispose;
  object_class->get_property = manuals_search_model_get_property;
  object_class->set_property = manuals_search_model_set_property;

  properties[PROP_GROUP] =
    g_param_spec_object ("group", NULL, NULL,
                         GOM_TYPE_RESOURCE_GROUP,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_search_model_init (ManualsSearchModel *self)
{
  self->prefetch = g_ptr_array_new_with_free_func (_dex_xunref);
  self->items = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}
