/*
 * manuals-navigatable-model.c
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

#include "manuals-navigatable-model.h"

struct _ManualsNavigatableModel
{
  GObject parent_instance;
  ManualsNavigatable *navigatable;
  DexFuture *children;
};

enum {
  PROP_0,
  PROP_NAVIGATABLE,
  N_PROPS
};

static DexFuture *
manuals_navigatable_model_setup (DexFuture *completed,
                                 gpointer   user_data)
{
  ManualsNavigatableModel *self = user_data;
  g_autoptr(GListModel) model = NULL;
  guint n_items;

  g_assert (MANUALS_IS_NAVIGATABLE_MODEL (self));

  model = dex_await_object (dex_ref (completed), NULL);

  g_signal_connect_object (model,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (model);

  if (n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, n_items);

  return dex_ref (completed);
}

static GListModel *
manuals_navigatable_model_dup_children (ManualsNavigatableModel *self)
{
  g_assert (MANUALS_IS_NAVIGATABLE_MODEL (self));

  if (self->children == NULL)
    {
      self->children = manuals_navigatable_find_children (self->navigatable);
      dex_future_disown (dex_future_then (dex_ref (self->children),
                                          manuals_navigatable_model_setup,
                                          g_object_ref (self),
                                          g_object_unref));
    }

  if (!dex_future_is_resolved (self->children))
    return NULL;

  return dex_await_object (dex_ref (self->children), NULL);
}

static guint
manuals_navigatable_model_get_n_items (GListModel *model)
{
  ManualsNavigatableModel *self = MANUALS_NAVIGATABLE_MODEL (model);
  g_autoptr(GListModel) children = manuals_navigatable_model_dup_children (self);

  if (children == NULL)
    return 0;

  return g_list_model_get_n_items (children);
}

static gpointer
manuals_navigatable_model_get_item (GListModel *model,
                                    guint       position)
{
  ManualsNavigatableModel *self = MANUALS_NAVIGATABLE_MODEL (model);
  g_autoptr(GListModel) children = manuals_navigatable_model_dup_children (self);

  if (children == NULL)
    return NULL;

  return g_list_model_get_item (children, position);
}

static GType
manuals_navigatable_model_get_item_type (GListModel *model)
{
  return MANUALS_TYPE_NAVIGATABLE;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = manuals_navigatable_model_get_n_items;
  iface->get_item = manuals_navigatable_model_get_item;
  iface->get_item_type = manuals_navigatable_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsNavigatableModel, manuals_navigatable_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

static void
manuals_navigatable_model_finalize (GObject *object)
{
  ManualsNavigatableModel *self = (ManualsNavigatableModel *)object;

  g_clear_object (&self->navigatable);

  G_OBJECT_CLASS (manuals_navigatable_model_parent_class)->finalize (object);
}

static void
manuals_navigatable_model_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ManualsNavigatableModel *self = MANUALS_NAVIGATABLE_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      g_value_set_object (value, self->navigatable);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_navigatable_model_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ManualsNavigatableModel *self = MANUALS_NAVIGATABLE_MODEL (object);

  switch (prop_id)
    {
    case PROP_NAVIGATABLE:
      self->navigatable = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_navigatable_model_class_init (ManualsNavigatableModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_navigatable_model_finalize;
  object_class->get_property = manuals_navigatable_model_get_property;
  object_class->set_property = manuals_navigatable_model_set_property;

  properties[PROP_NAVIGATABLE] =
    g_param_spec_object ("navigatable", NULL, NULL,
                         MANUALS_TYPE_NAVIGATABLE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_navigatable_model_init (ManualsNavigatableModel *self)
{
}

ManualsNavigatableModel *
manuals_navigatable_model_new (ManualsNavigatable *navigatable)
{
  g_return_val_if_fail (MANUALS_IS_NAVIGATABLE (navigatable), NULL);

  return g_object_new (MANUALS_TYPE_NAVIGATABLE_MODEL,
                       "navigatable", navigatable,
                       NULL);
}
