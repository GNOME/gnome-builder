/* ide-unique-list-model.c
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

#define G_LOG_DOMAIN "ide-unique-list-model"

#include "config.h"

#include "ide-unique-list-model.h"

struct _IdeUniqueListModel
{
  GObject     parent_instance;

  GListModel *model;
  GtkSorter  *sorter;

  GtkBitset  *unique;
  GtkBitset  *pending;

  guint       incremental_source;

  guint       incremental : 1;
};

enum {
  PROP_0,
  PROP_INCREMENTAL,
  PROP_MODEL,
  PROP_SORTER,
  N_PROPS
};

static GType
ide_unique_list_model_get_item_type (GListModel *model)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (model);

  if (self->model != NULL)
    return g_list_model_get_item_type (self->model);

  return G_TYPE_OBJECT;
}

static guint
ide_unique_list_model_get_n_items (GListModel *model)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (model);

  return gtk_bitset_get_size (self->unique);
}

static gpointer
ide_unique_list_model_get_item (GListModel *model,
                                guint       position)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (model);
  guint unfiltered = gtk_bitset_get_nth (self->unique, position);

  if (unfiltered == 0 && position >= gtk_bitset_get_size (self->unique))
    return NULL;

  return g_list_model_get_item (self->model, unfiltered);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_unique_list_model_get_item_type;
  iface->get_n_items = ide_unique_list_model_get_n_items;
  iface->get_item = ide_unique_list_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeUniqueListModel, ide_unique_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_unique_list_model_dispose (GObject *object)
{
  IdeUniqueListModel *self = (IdeUniqueListModel *)object;

  g_clear_handle_id (&self->incremental_source, g_source_remove);

  g_clear_object (&self->model);
  g_clear_object (&self->sorter);
  g_clear_pointer (&self->unique, gtk_bitset_unref);

  G_OBJECT_CLASS (ide_unique_list_model_parent_class)->dispose (object);
}

static void
ide_unique_list_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_INCREMENTAL:
      g_value_set_boolean (value, ide_unique_list_model_get_incremental (self));
      break;

    case PROP_MODEL:
      g_value_set_object (value, ide_unique_list_model_get_model (self));
      break;

    case PROP_SORTER:
      g_value_set_object (value, ide_unique_list_model_get_sorter (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_unique_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_INCREMENTAL:
      ide_unique_list_model_set_incremental (self, g_value_get_boolean (value));
      break;

    case PROP_MODEL:
      ide_unique_list_model_set_model (self, g_value_get_object (value));
      break;

    case PROP_SORTER:
      ide_unique_list_model_set_sorter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_unique_list_model_class_init (IdeUniqueListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_unique_list_model_dispose;
  object_class->get_property = ide_unique_list_model_get_property;
  object_class->set_property = ide_unique_list_model_set_property;

  properties[PROP_INCREMENTAL] =
    g_param_spec_boolean ("incremental", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SORTER] =
    g_param_spec_object ("sorter", NULL, NULL,
                         GTK_TYPE_SORTER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_unique_list_model_init (IdeUniqueListModel *self)
{
}

/**
 * ide_unique_list_model_new:
 * @model: (transfer full) (nullable): a #GListModel or %NULL
 * @sorter: (transfer full) (nullable): a #GtkSorter or %NULL
 *
 * Creates a new #IdeUniqueListModel which can deduplicate items
 * which are sequential.
 *
 * Returns: (transfer full): an #IdeUniqueListModel
 *
 * Since: 44
 */
IdeUniqueListModel *
ide_unique_list_model_new (GListModel *model,
                           GtkSorter  *sorter)
{
  IdeUniqueListModel *ret;

  g_return_val_if_fail (!model || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (!sorter || GTK_IS_SORTER (sorter), NULL);

  ret = g_object_new (IDE_TYPE_UNIQUE_LIST_MODEL,
                      "model", model,
                      "sorter", sorter,
                      NULL);

  g_clear_object (&model);
  g_clear_object (&sorter);

  return ret;
}

/**
 * ide_unique_list_model_get_model:
 * @self: a #IdeUniqueListModel
 *
 * Gets the underlying model.
 *
 * Returns: (transfer none) (nullable): a #GListModel
 *
 * Since: 44
 */
GListModel *
ide_unique_list_model_get_model (IdeUniqueListModel *self)
{
  g_return_val_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * ide_unique_list_model_set_model:
 * @self: a #IdeUniqueListModel
 * @model: (nullable): a #GListModel or %NULL
 *
 * Sets the underlying model to be deduplicated.
 *
 * Since: 44
 */
void
ide_unique_list_model_set_model (IdeUniqueListModel *self,
                                 GListModel         *model)
{
  g_return_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
}

/**
 * ide_unique_list_model_get_sorter:
 * @self: a #IdeUniqueListModel
 *
 * Get the #GtkSorter used to deduplicate items.
 *
 * Returns: (transfer none) (nullable): a GtkSorter or %NULL
 *
 * Since: 44
 */
GtkSorter *
ide_unique_list_model_get_sorter (IdeUniqueListModel *self)
{
  g_return_val_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self), NULL);

  return self->sorter;
}

/**
 * ide_unique_list_model_set_sorter:
 * @self: a #IdeUniqueListModel
 * @sorter: (nullable): a #GtkSorter or %NULL
 *
 * Sets the sorter used to deduplicate items.
 *
 * Since: 44
 */
void
ide_unique_list_model_set_sorter (IdeUniqueListModel *self,
                                  GtkSorter          *sorter)
{
  g_return_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self));
  g_return_if_fail (!sorter || GTK_IS_SORTER (sorter));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SORTER]);
}

/**
 * ide_unique_list_model_get_incremental:
 * @self: a #IdeUniqueListModel
 *
 * Since: 44
 */
gboolean
ide_unique_list_model_get_incremental (IdeUniqueListModel *self)
{
  g_return_val_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self), FALSE);

  return self->incremental;
}

/**
 * ide_unique_list_model_set_incremental:
 * @self: a #IdeUniqueListModel
 * @incremental: if filtering should be incremental
 *
 * Since: 44
 */
void
ide_unique_list_model_set_incremental (IdeUniqueListModel *self,
                                       gboolean            incremental)
{
  g_return_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self));

  incremental = !!incremental;

  if (self->incremental != incremental)
    {
      self->incremental = incremental;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INCREMENTAL]);
    }
}
