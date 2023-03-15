/*
 * Copyright © 2018 Benjamin Otte
 * Copyright © 2023 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Christian Hergert <chergert@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "ide-unique-list-model"

#include "config.h"

#include "ide-unique-list-model.h"

struct _IdeUniqueListModel
{
  GObject           parent_instance;

  GtkSortListModel *sorted;
  GtkSorter        *sorter;

  GtkBitset        *unique;
  GtkBitset        *pending;

  guint             incremental_source;
  guint             incremental : 1;
};

enum {
  PROP_0,
  PROP_INCREMENTAL,
  PROP_MODEL,
  PROP_N_ITEMS,
  PROP_PENDING,
  PROP_SORTER,
  N_PROPS
};

static GType
ide_unique_list_model_get_item_type (GListModel *model)
{
  IdeUniqueListModel *self = IDE_UNIQUE_LIST_MODEL (model);

  if (self->sorted != NULL)
    return g_list_model_get_item_type (G_LIST_MODEL (self->sorted));

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

  return g_list_model_get_item (G_LIST_MODEL (self->sorted), unfiltered);
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

static gboolean
ide_unique_list_model_run_filter_on_item (IdeUniqueListModel *self,
                                          guint               position)
{
  gpointer previous;
  gpointer item;
  gboolean visible;

  if (position == 0 || self->sorter == NULL)
    return TRUE;

  previous = g_list_model_get_item (G_LIST_MODEL (self->sorted), position - 1);
  item = g_list_model_get_item (G_LIST_MODEL (self->sorted), position);

  visible = gtk_sorter_compare (self->sorter, previous, item) != 0;

  g_object_unref (previous);
  g_object_unref (item);

  return visible;
}

static void
ide_unique_list_model_run_deduplicator (IdeUniqueListModel *self,
                                        guint               n_steps)
{
  GtkBitsetIter iter;
  guint i, pos;
  gboolean more;

  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));

  if (self->pending == NULL)
    return;

  for (i = 0, more = gtk_bitset_iter_init_first (&iter, self->pending, &pos);
       i < n_steps && more;
       i++, more = gtk_bitset_iter_next (&iter, &pos))
    {
      if (ide_unique_list_model_run_filter_on_item (self, pos))
        gtk_bitset_add (self->unique, pos);
    }

  if (more)
    gtk_bitset_remove_range_closed (self->pending, 0, pos - 1);
  else
    g_clear_pointer (&self->pending, gtk_bitset_unref);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
}

static void
ide_unique_list_model_stop_deduplicating (IdeUniqueListModel *self)
{
  gboolean notify_pending;

  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));

  notify_pending = self->pending != NULL;

  g_clear_pointer (&self->pending, gtk_bitset_unref);
  g_clear_handle_id (&self->incremental_source, g_source_remove);

  if (notify_pending)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
}

/**
 * ide_unique_list_model_emit_items_changed_for_changes:
 * @old: (transfer full): a #GtkBitset
 */
static void
ide_unique_list_model_emit_items_changed_for_changes (IdeUniqueListModel *self,
                                                      GtkBitset          *old)
{
  GtkBitset *changes;

  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));
  g_assert (old != NULL);

  changes = gtk_bitset_copy (self->unique);

  gtk_bitset_difference (changes, old);

  if (!gtk_bitset_is_empty (changes))
    {
      guint min, max, removed, added;

      min = gtk_bitset_get_minimum (changes);
      max = gtk_bitset_get_maximum (changes);
      removed = gtk_bitset_get_size_in_range (old, min, max);
      added = gtk_bitset_get_size_in_range (self->unique, min, max);

      g_list_model_items_changed (G_LIST_MODEL (self),
                                  min == 0 ? 0 : gtk_bitset_get_size_in_range (self->unique, 0, min - 1),
                                  removed,
                                  added);

      if (removed != added)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
    }

  gtk_bitset_unref (changes);
  gtk_bitset_unref (old);
}

static gboolean
ide_unique_list_model_deduplicate_source (gpointer user_data)
{
  IdeUniqueListModel *self = user_data;
  GtkBitset *old;

  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));
  g_assert (self->unique != NULL);

  old = gtk_bitset_copy (self->unique);
  ide_unique_list_model_run_deduplicator (self, 512);

  if (self->pending == NULL)
    ide_unique_list_model_stop_deduplicating (self);

  ide_unique_list_model_emit_items_changed_for_changes (self, old);

  return G_SOURCE_CONTINUE;
}

/**
 * ide_unique_list_model_start_deduplicating:
 * @items: (transfer full): a #GtkBitset
 */
static void
ide_unique_list_model_start_deduplicating (IdeUniqueListModel *self,
                                           GtkBitset          *items)
{
  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));
  g_assert (items != NULL);

  if (self->pending != NULL)
    {
      gtk_bitset_union (self->pending, items);
      gtk_bitset_unref (items);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
      return;
    }

  if (gtk_bitset_is_empty (items))
    {
      gtk_bitset_unref (items);
      return;
    }

  self->pending = g_steal_pointer (&items);

  if (!self->incremental)
    {
      ide_unique_list_model_run_deduplicator (self, G_MAXUINT);
      g_assert (self->pending == NULL);
      return;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);

  g_assert (self->incremental_source == 0);

  self->incremental_source = g_idle_add (ide_unique_list_model_deduplicate_source, self);
  g_source_set_static_name (g_main_context_find_source_by_id (NULL,
                                                              self->incremental_source),
                            "[ide] ide_unique_list_model_deduplicate_source");
}

static void
ide_unique_list_model_sorted_items_changed_cb (IdeUniqueListModel *self,
                                               guint               position,
                                               guint               removed,
                                               guint               added,
                                               GtkSortListModel   *sorted)
{
  guint sorter_removed = 0;
  guint sorter_added = 0;

  g_assert (IDE_IS_UNIQUE_LIST_MODEL (self));

  if (removed == 0 && added == 0)
    return;

  if (removed > 0)
    sorter_removed = gtk_bitset_get_size_in_range (self->unique, position, position + removed - 1);

  gtk_bitset_splice (self->unique, position, removed, added);
  if (self->pending != NULL)
    gtk_bitset_splice (self->pending, position, removed, added);

  if (added > 0)
    {
      gboolean has_tail = position + added < g_list_model_get_n_items (G_LIST_MODEL (sorted));

      /* We have to look at the next item too so that we can be sure that only the first
       * of the adjacent items are displayed.
       */

      ide_unique_list_model_start_deduplicating (self, gtk_bitset_new_range (position, added + has_tail));
      sorter_added = gtk_bitset_get_size_in_range (self->unique, position, position + added - 1);
    }

  if (sorter_removed > 0 || sorter_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self),
                                position == 0 ? 0 : gtk_bitset_get_size_in_range (self->unique, 0, position - 1),
                                sorter_removed,
                                sorter_added);

  if (sorter_removed != sorter_added)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ITEMS]);
}

static void
ide_unique_list_model_dispose (GObject *object)
{
  IdeUniqueListModel *self = (IdeUniqueListModel *)object;

  g_clear_object (&self->sorter);

  gtk_sort_list_model_set_model (self->sorted, NULL);
  gtk_sort_list_model_set_sorter (self->sorted, NULL);

  g_clear_handle_id (&self->incremental_source, g_source_remove);
  g_clear_pointer (&self->pending, gtk_bitset_unref);

  G_OBJECT_CLASS (ide_unique_list_model_parent_class)->dispose (object);
}

static void
ide_unique_list_model_finalize (GObject *object)
{
  IdeUniqueListModel *self = (IdeUniqueListModel *)object;

  g_clear_object (&self->sorted);
  g_clear_pointer (&self->unique, gtk_bitset_unref);

  G_OBJECT_CLASS (ide_unique_list_model_parent_class)->finalize (object);
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

    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (G_LIST_MODEL (self)));
      break;

    case PROP_PENDING:
      g_value_set_uint (value, ide_unique_list_model_get_pending (self));
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
  object_class->finalize = ide_unique_list_model_finalize;
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

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_PENDING] =
    g_param_spec_uint ("pending", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SORTER] =
    g_param_spec_object ("sorter", NULL, NULL,
                         GTK_TYPE_SORTER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_unique_list_model_init (IdeUniqueListModel *self)
{
  self->unique = gtk_bitset_new_empty ();
  self->sorted = gtk_sort_list_model_new (NULL, NULL);
  gtk_sort_list_model_set_incremental (self->sorted, FALSE);
  g_signal_connect_object (self->sorted,
                           "items-changed",
                           G_CALLBACK (ide_unique_list_model_sorted_items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
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

  return gtk_sort_list_model_get_model (self->sorted);
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

  if (model == ide_unique_list_model_get_model (self))
    return;

  gtk_sort_list_model_set_model (self->sorted, model);

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

  if (g_set_object (&self->sorter, sorter))
    {
      gtk_sort_list_model_set_sorter (self->sorted, sorter);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SORTER]);
    }
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
      gtk_sort_list_model_set_incremental (self->sorted, incremental);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INCREMENTAL]);
    }
}

/**
 * ide_unique_list_model_get_pending:
 * @self: an #IdeUniqueListModel
 *
 * Returns the number of items that have not yet been filtered.
 *
 * You can use this value to check if @self is busy filtering by
 * comparing the return value to 0 or you can compute the percentage
 * of the filter remaining by dividing the return value by the total
 * number of items in the underlying model:
 *
 * Returns: The number of items noet yet filtered
 */
guint
ide_unique_list_model_get_pending (IdeUniqueListModel *self)
{
  g_return_val_if_fail (IDE_IS_UNIQUE_LIST_MODEL (self), 0);

  if (self->pending == NULL)
    return 0;

  return gtk_bitset_get_size (self->pending);
}
