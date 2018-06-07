/* ide-langserv-completion-results.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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
 */

#include "config.h"

#define G_LOG_DOMAIN "ide-langserv-completion-results.h"

#include "ide-debug.h"

#include "completion/ide-completion.h"
#include "langserv/ide-langserv-completion-item.h"
#include "langserv/ide-langserv-completion-results.h"

struct _IdeLangservCompletionResults
{
  GObject   parent_instance;
  GVariant *results;
  GArray   *items;
};

typedef struct
{
  guint index;
  guint priority;
} Item;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeLangservCompletionResults, ide_langserv_completion_results, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_langserv_completion_results_finalize (GObject *object)
{
  IdeLangservCompletionResults *self = (IdeLangservCompletionResults *)object;

  g_clear_pointer (&self->results, g_variant_unref);
  g_clear_pointer (&self->items, g_array_unref);

  G_OBJECT_CLASS (ide_langserv_completion_results_parent_class)->finalize (object);
}

static void
ide_langserv_completion_results_class_init (IdeLangservCompletionResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_completion_results_finalize;
}

static void
ide_langserv_completion_results_init (IdeLangservCompletionResults *self)
{
  self->items = g_array_new (FALSE, FALSE, sizeof (Item));
}

IdeLangservCompletionResults *
ide_langserv_completion_results_new (GVariant *results)
{
  IdeLangservCompletionResults *self;

  g_return_val_if_fail (results != NULL, NULL);

  self = g_object_new (IDE_TYPE_LANGSERV_COMPLETION_RESULTS, NULL);
  self->results = g_variant_ref_sink (results);

  ide_langserv_completion_results_refilter (self, NULL);

  return self;
}

static GType
ide_langserv_completion_results_get_item_type (GListModel *model)
{
  return IDE_TYPE_LANGSERV_COMPLETION_ITEM;
}

static guint
ide_langserv_completion_results_get_n_items (GListModel *model)
{
  IdeLangservCompletionResults *self = (IdeLangservCompletionResults *)model;

  g_assert (IDE_IS_LANGSERV_COMPLETION_RESULTS (self));

  return self->items->len;
}

static gpointer
ide_langserv_completion_results_get_item (GListModel *model,
                                          guint       position)
{
  IdeLangservCompletionResults *self = (IdeLangservCompletionResults *)model;
  g_autoptr(GVariant) child = NULL;
  const Item *item;

  g_assert (IDE_IS_LANGSERV_COMPLETION_RESULTS (self));
  g_assert (self->results != NULL);

  item = &g_array_index (self->items, Item, position);
  child = g_variant_get_child_value (self->results, item->index);

  return ide_langserv_completion_item_new (child);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = ide_langserv_completion_results_get_item;
  iface->get_n_items = ide_langserv_completion_results_get_n_items;
  iface->get_item_type = ide_langserv_completion_results_get_item_type;
}

static gint
compare_items (const Item *a,
               const Item *b)
{
  return (gint)a->priority - (gint)b->priority;
}

void
ide_langserv_completion_results_refilter (IdeLangservCompletionResults *self,
                                          const gchar                  *typed_text)
{
  g_autofree gchar *query = NULL;
  GVariantIter iter;
  GVariant *node;
  guint index = 0;
  guint old_len;

  g_return_if_fail (IDE_IS_LANGSERV_COMPLETION_RESULTS (self));

  if ((old_len = self->items->len))
    g_array_remove_range (self->items, 0, old_len);

  if (self->results == NULL)
    return;

  if (typed_text == NULL || *typed_text == 0)
    {
      guint n_items = g_variant_n_children (self->results);

      for (guint i = 0; i < n_items; i++)
        {
          Item item = { .index = i };
          g_array_append_val (self->items, item);
        }

      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, n_items);

      return;
    }

  query = g_utf8_strdown (typed_text, -1);

  g_variant_iter_init (&iter, self->results);

  while (g_variant_iter_loop (&iter, "v", &node))
    {
      const gchar *label;
      guint priority;

      if (!g_variant_lookup (node, "label", "&s", &label))
        continue;

      if (ide_completion_fuzzy_match (label, query, &priority))
        {
          Item item = { .index = index, .priority = priority };
          g_array_append_val (self->items, item);
        }

      index++;
    }

  g_array_sort (self->items, (GCompareFunc)compare_items);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, index);
}
