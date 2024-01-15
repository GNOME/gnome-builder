/* ide-truncate-model.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-truncate-model"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-truncate-model.h"

#define DEFAULT_MAX_ITEMS 4

struct _IdeTruncateModel
{
  GObject            parent_instance;
  GListModel        *child_model;
  GtkSliceListModel *slice;
  guint              max_items;
  guint              expanded : 1;
};

static gpointer
ide_truncate_model_get_item (GListModel *model,
                             guint       position)
{
  IdeTruncateModel *self = IDE_TRUNCATE_MODEL (model);

  return self->expanded ? g_list_model_get_item (self->child_model, position)
                        : g_list_model_get_item (G_LIST_MODEL (self->slice), position);
}

static guint
ide_truncate_model_get_n_items (GListModel *model)
{
  IdeTruncateModel *self = (IdeTruncateModel *)model;

  return self->expanded ? g_list_model_get_n_items (G_LIST_MODEL (self->child_model))
                        : g_list_model_get_n_items (G_LIST_MODEL (self->slice));
}

static GType
ide_truncate_model_get_item_type (GListModel *model)
{
  return g_list_model_get_item_type (IDE_TRUNCATE_MODEL (model)->child_model);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = ide_truncate_model_get_item;
  iface->get_n_items = ide_truncate_model_get_n_items;
  iface->get_item_type = ide_truncate_model_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeTruncateModel, ide_truncate_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_CAN_EXPAND,
  PROP_CHILD_MODEL,
  PROP_EXPANDED,
  PROP_MAX_ITEMS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_truncate_model_new:
 * @child_model: a #GListModel
 *
 * Create a new #IdeTruncateModel that wraps @child_model. Only
 * #IdeTruncateModel:max-items will be displayed until
 * #IdeTrunicateModel:expanded is set.
 *
 * Returns: (transfer full): a newly created #IdeTruncateModel
 */
IdeTruncateModel *
ide_truncate_model_new (GListModel *child_model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (child_model), NULL);

  return g_object_new (IDE_TYPE_TRUNCATE_MODEL,
                       "child-model", child_model,
                       NULL);
}

static void
ide_truncate_model_items_changed_cb (IdeTruncateModel *self,
                                     guint             position,
                                     guint             removed,
                                     guint             added,
                                     GListModel       *model)
{
  g_assert (IDE_IS_TRUNCATE_MODEL (self));
  g_assert (G_IS_LIST_MODEL (model));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_EXPAND]);
}

static void
ide_truncate_model_finalize (GObject *object)
{
  IdeTruncateModel *self = (IdeTruncateModel *)object;

  g_clear_object (&self->child_model);
  g_clear_object (&self->slice);

  G_OBJECT_CLASS (ide_truncate_model_parent_class)->finalize (object);
}

static void
ide_truncate_model_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeTruncateModel *self = IDE_TRUNCATE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CAN_EXPAND:
      g_value_set_boolean (value, ide_truncate_model_get_can_expand (self));
      break;

    case PROP_CHILD_MODEL:
      g_value_set_object (value, ide_truncate_model_get_child_model (self));
      break;

    case PROP_MAX_ITEMS:
      g_value_set_uint (value, ide_truncate_model_get_max_items (self));
      break;

    case PROP_EXPANDED:
      g_value_set_boolean (value, ide_truncate_model_get_expanded (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_truncate_model_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeTruncateModel *self = IDE_TRUNCATE_MODEL (object);

  switch (prop_id)
    {
    case PROP_CHILD_MODEL:
      self->child_model = g_value_dup_object (value);
      gtk_slice_list_model_set_model (self->slice, self->child_model);
      break;

    case PROP_MAX_ITEMS:
      ide_truncate_model_set_max_items (self, g_value_get_uint (value));
      break;

    case PROP_EXPANDED:
      ide_truncate_model_set_expanded (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_truncate_model_class_init (IdeTruncateModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_truncate_model_finalize;
  object_class->get_property = ide_truncate_model_get_property;
  object_class->set_property = ide_truncate_model_set_property;

  properties [PROP_CAN_EXPAND] =
    g_param_spec_boolean ("can-expand",
                          "Can Expand",
                          "If the model can be expanded",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTruncateModel:child-model:
   *
   * The "child-model" property is the model to be trunicated.
   */
  properties [PROP_CHILD_MODEL] =
    g_param_spec_object ("child-model",
                         "Child Model",
                         "Child GListModel",
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MAX_ITEMS] =
    g_param_spec_uint ("max-items",
                       "Max Items",
                       "Max items to display when not expanded",
                       0, G_MAXUINT, DEFAULT_MAX_ITEMS,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_EXPANDED] =
    g_param_spec_boolean ("expanded",
                          "Expanded",
                          "If all the items should be displayed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_truncate_model_init (IdeTruncateModel *self)
{
  self->max_items = DEFAULT_MAX_ITEMS;
  self->slice = gtk_slice_list_model_new (NULL, 0, DEFAULT_MAX_ITEMS);

  g_signal_connect_object (self->slice,
                           "items-changed",
                           G_CALLBACK (ide_truncate_model_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

gboolean
ide_truncate_model_get_expanded (IdeTruncateModel *self)
{
  g_return_val_if_fail (IDE_IS_TRUNCATE_MODEL (self), FALSE);

  return self->expanded;
}

void
ide_truncate_model_set_expanded (IdeTruncateModel *self,
                                 gboolean          expanded)
{
  g_return_if_fail (IDE_IS_TRUNCATE_MODEL (self));

  expanded = !!expanded;

  if (expanded != self->expanded)
    {
      self->expanded = expanded;

      gtk_slice_list_model_set_size (self->slice,
                                     self->expanded ? G_MAXUINT : self->max_items);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXPANDED]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_EXPAND]);
    }
}

guint
ide_truncate_model_get_max_items (IdeTruncateModel *self)
{
  g_return_val_if_fail (IDE_IS_TRUNCATE_MODEL (self), 0);

  return self->max_items;
}

void
ide_truncate_model_set_max_items (IdeTruncateModel *self,
                                  guint             max_items)
{
  g_return_if_fail (IDE_IS_TRUNCATE_MODEL (self));

  if (max_items != self->max_items)
    {
      self->max_items = max_items;

      if (!self->expanded)
        gtk_slice_list_model_set_size (self->slice, max_items);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_ITEMS]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_EXPAND]);
    }
}

/**
 * ide_truncate_model_get_child_model:
 *
 * Gets the #IdeTruncateModel:child-model property.
 *
 * Returns: (transfer none): a #GListModel
 */
GListModel *
ide_truncate_model_get_child_model (IdeTruncateModel *self)
{
  g_return_val_if_fail (IDE_IS_TRUNCATE_MODEL (self), NULL);

  return self->child_model;
}

gboolean
ide_truncate_model_get_can_expand (IdeTruncateModel *self)
{
  g_return_val_if_fail (IDE_IS_TRUNCATE_MODEL (self), FALSE);

  return !self->expanded &&
         g_list_model_get_n_items (self->child_model) != g_list_model_get_n_items (G_LIST_MODEL (self->slice));
}
