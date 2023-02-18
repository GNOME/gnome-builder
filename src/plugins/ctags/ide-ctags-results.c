/* ide-ctags-results.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-ctags-results"

#include "ide-ctags-completion-item.h"
#include "ide-ctags-results.h"
#include "ide-ctags-util.h"

struct _IdeCtagsResults
{
  GObject parent_instance;
  const gchar * const *suffixes;
  GCancellable *refilter_cancellable;
  gchar *word;
  GPtrArray *indexes;
  GArray *items;
};

typedef struct
{
  const IdeCtagsIndexEntry *entry;
  guint priority;
} Item;

typedef struct
{
  const gchar * const *suffixes;
  gchar *word;
  gchar *casefold;
  GPtrArray *indexes;
  GArray *items;
} Populate;

static void
populate_free (Populate *state)
{
  g_clear_pointer (&state->word, g_free);
  g_clear_pointer (&state->casefold, g_free);
  g_clear_pointer (&state->indexes, g_ptr_array_unref);
  g_clear_pointer (&state->items, g_array_unref);
  g_slice_free (Populate, state);
}

static GType
ide_ctags_results_get_item_type (GListModel *model)
{
  return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static gpointer
ide_ctags_results_get_item (GListModel *model,
                            guint       position)
{
  IdeCtagsResults *self = (IdeCtagsResults *)model;
  const Item *item;

  g_assert (IDE_IS_CTAGS_RESULTS (self));

  if (position >= self->items->len)
    return NULL;

  item = &g_array_index (self->items, Item, position);

  return ide_ctags_completion_item_new (self, item->entry);
}

static guint
ide_ctags_results_get_n_items (GListModel *model)
{
  g_assert (IDE_IS_CTAGS_RESULTS (model));

  return IDE_CTAGS_RESULTS (model)->items->len;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_ctags_results_get_item_type;
  iface->get_n_items = ide_ctags_results_get_n_items;
  iface->get_item = ide_ctags_results_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCtagsResults, ide_ctags_results, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_ctags_results_finalize (GObject *object)
{
  IdeCtagsResults *self = (IdeCtagsResults *)object;

  g_clear_object (&self->refilter_cancellable);
  g_clear_pointer (&self->items, g_array_unref);
  g_clear_pointer (&self->indexes, g_ptr_array_unref);
  g_clear_pointer (&self->word, g_free);

  G_OBJECT_CLASS (ide_ctags_results_parent_class)->finalize (object);
}

static void
ide_ctags_results_class_init (IdeCtagsResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_results_finalize;
}

static void
ide_ctags_results_init (IdeCtagsResults *self)
{
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->items = g_array_new (FALSE, FALSE, sizeof (Item));
}

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  return (gint)((const Item *)a)->priority -
         (gint)((const Item *)b)->priority;
}

static void
ide_ctags_results_populate_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  Populate *p = task_data;
  g_autoptr(GHashTable) completions = NULL;
  guint word_len;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_RESULTS (source_object));
  g_assert (p != NULL);
  g_assert (p->word != NULL);
  g_assert (p->casefold != NULL);
  g_assert (p->indexes != NULL);
  g_assert (p->items != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  completions = g_hash_table_new (g_str_hash, g_str_equal);
  word_len = strlen (p->word);

  for (guint i = 0; i < p->indexes->len; i++)
    {
      g_autofree gchar *copy = g_strdup (p->word);
      IdeCtagsIndex *index = g_ptr_array_index (p->indexes, i);
      const IdeCtagsIndexEntry *entries = NULL;
      guint tmp_len = word_len;
      gsize n_entries = 0;

      while (!entries && *copy)
        {
          if (!(entries = ide_ctags_index_lookup_prefix (index, copy, &n_entries)))
            copy [--tmp_len] = '\0';
        }

      if (!entries || !n_entries)
        continue;

      for (guint j = 0; j < n_entries; j++)
        {
          const IdeCtagsIndexEntry *entry = &entries [j];
          guint priority;

          if (g_hash_table_contains (completions, entry->name))
            continue;

          g_hash_table_add (completions, (gchar *)entry->name);

          if (!ide_ctags_is_allowed (entry, p->suffixes))
            continue;

          if (gtk_source_completion_fuzzy_match (entry->name, p->casefold, &priority))
            {
              Item item;

              item.entry = entry;
              item.priority = priority;

              g_array_append_val (p->items, item);
            }
        }
    }

  g_array_sort (p->items, sort_by_priority);

  ide_task_return_pointer (task,
                           g_array_ref (p->items),
                           g_array_unref);
}

#if 0

  casefold = g_utf8_casefold (word, -1);
  store = g_list_store_new (IDE_TYPE_COMPLETION_PROPOSAL);

  *results = g_object_ref (G_LIST_MODEL (store));

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

#endif

void
ide_ctags_results_populate_async (IdeCtagsResults     *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Populate *p;

  g_return_if_fail (IDE_IS_CTAGS_RESULTS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_results_populate_async);
  ide_task_set_priority (task, G_PRIORITY_HIGH);
  ide_task_set_complete_priority (task, G_PRIORITY_LOW);

  if (self->word == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "No word set to query");
      return;
    }

  p = g_slice_new (Populate);
  p->word = g_strdup (self->word);
  p->casefold = g_utf8_casefold (self->word, -1);
  p->indexes = g_ptr_array_new_full (self->indexes->len, g_object_unref);
  p->items = g_array_new (FALSE, FALSE, sizeof (GArray));
  p->suffixes = self->suffixes;

  for (guint i = 0; i < self->indexes->len; i++)
    g_ptr_array_add (p->indexes, g_object_ref (g_ptr_array_index (self->indexes, i)));

  ide_task_set_task_data (task, p, populate_free);
  ide_task_run_in_thread (task, ide_ctags_results_populate_worker);
}

gboolean
ide_ctags_results_populate_finish (IdeCtagsResults  *self,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  GArray *items;

  g_return_val_if_fail (IDE_IS_CTAGS_RESULTS (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  if ((items = ide_task_propagate_pointer (IDE_TASK (result), error)))
    {
      guint old_len = self->items->len;
      guint new_len = items->len;

      g_array_unref (self->items);
      self->items = g_steal_pointer (&items);

      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, new_len);

      return TRUE;
    }

  return FALSE;
}

void
ide_ctags_results_set_suffixes (IdeCtagsResults     *self,
                                const gchar * const *suffixes)
{
  /* @suffixes is an interned string array */
  self->suffixes = suffixes;
}

void
ide_ctags_results_set_word (IdeCtagsResults *self,
                            const gchar     *word)
{
  g_assert (IDE_IS_CTAGS_RESULTS (self));

  g_set_str (&self->word, word);
}

IdeCtagsResults *
ide_ctags_results_new (void)
{
  return g_object_new (IDE_TYPE_CTAGS_RESULTS, NULL);
}

void
ide_ctags_results_add_index (IdeCtagsResults *self,
                             IdeCtagsIndex   *index)
{
  g_assert (IDE_IS_CTAGS_RESULTS (self));
  g_assert (IDE_IS_CTAGS_INDEX (index));

  g_ptr_array_add (self->indexes, g_object_ref (index));
}

static void
ide_ctags_results_refilter_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeCtagsResults *self = (IdeCtagsResults *)object;

  g_assert (IDE_IS_CTAGS_RESULTS (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (ide_ctags_results_populate_finish (self, result, NULL))
    g_clear_object (&self->refilter_cancellable);
}

void
ide_ctags_results_refilter (IdeCtagsResults *self)
{
  g_return_if_fail (IDE_IS_CTAGS_RESULTS (self));

  g_cancellable_cancel (self->refilter_cancellable);
  self->refilter_cancellable = g_cancellable_new();

  ide_ctags_results_populate_async (self,
                                    self->refilter_cancellable,
                                    ide_ctags_results_refilter_cb,
                                    NULL);
}
