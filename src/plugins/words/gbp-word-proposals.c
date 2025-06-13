/* gbp-word-proposals.c
 *
 * Copyright 2017 Umang Jain <mailumangjain@gmail.com>
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

#define G_LOG_DOMAIN "gbp-word-proposals"

#include "config.h"

#include <libide-sourceview.h>

#include "gbp-word-proposal.h"
#include "gbp-word-proposals.h"

struct _GbpWordProposals
{
  GObject parent_instance;

  /*
   * A list of all of the words that we've found so far. This is filtered
   * in followup gbp_word_proposals_refilter() requests based on what we
   * found during our scan.
   */
  GPtrArray *unfiltered;

  /*
   * A filtered list of items (and their priority score from fuzzy matching).
   * This directly relates to the APIs that are exposed via GListModel.
   */
  GArray *items;

  /*
   * This is our string chunk so that we can use larger allocations for
   * words instead of lots of small allocations. We of course have to
   * create small allocations when we extract from the text btree.
   */
  GStringChunk *words;

  /*
   * Because GStringChunk doesn't have a "contains" API for it's
   * g_string_chunk_insert_const() internal hashtable, we have to do
   * this manually to quickly know if we can ignore a word.
   */
  GHashTable *words_dedup;

  /*
   * The last word that was searched for. If our new word to filter has this
   * as a prefix, we can skip a rescan of the buffer and instead just filter
   * our already filtered results. This makes filtering faster with every
   * key-press rather than slowing down from heavy scanning.
   */
  gchar *last_word;
};

typedef struct
{
  const gchar *word;
  guint        priority;
} Item;

typedef struct
{
  GtkTextMark *mark;
  guint wrapped : 1;
} State;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWordProposals, gbp_word_proposals, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
state_free (gpointer data)
{
  State *state = data;

  gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (state->mark), state->mark);
  g_object_unref (state->mark);
  g_slice_free (State, state);
}

static void
gbp_word_proposals_finalize (GObject *object)
{
  GbpWordProposals *self = (GbpWordProposals *)object;

  g_clear_pointer (&self->unfiltered, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_array_unref);
  g_clear_pointer (&self->words_dedup, g_hash_table_unref);
  g_clear_pointer (&self->words, g_string_chunk_free);
  g_clear_pointer (&self->last_word, g_free);

  G_OBJECT_CLASS (gbp_word_proposals_parent_class)->finalize (object);
}

static void
gbp_word_proposals_class_init (GbpWordProposalsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_word_proposals_finalize;
}

static void
gbp_word_proposals_init (GbpWordProposals *self)
{
  self->items = g_array_new (FALSE, FALSE, sizeof (Item));
  self->unfiltered = g_ptr_array_new ();
  self->words = g_string_chunk_new (4096);
  self->words_dedup = g_hash_table_new (g_str_hash, g_str_equal);
}

GbpWordProposals *
gbp_word_proposals_new (void)
{
  return g_object_new (GBP_TYPE_WORD_PROPOSALS, NULL);
}

static void
gbp_word_proposals_add (GbpWordProposals *self,
                        const gchar      *word)
{
  g_assert (GBP_IS_WORD_PROPOSALS (self));
  g_assert (word != NULL);

  if (g_hash_table_contains (self->words_dedup, word))
    return;

  word = g_string_chunk_insert (self->words, word);
  g_ptr_array_add (self->unfiltered, (gchar *)word);
  g_hash_table_add (self->words_dedup, (gchar *)word);
}

static void
gbp_word_proposals_backward_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GtkSourceSearchContext *search = (GtkSourceSearchContext *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *word = NULL;
  GbpWordProposals *self;
  GtkTextBuffer *buffer;
  GCancellable *cancellable;
  GtkTextIter begin, end;
  gboolean wrapped = FALSE;
  State *state;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (search));

  if (gtk_source_search_context_backward_finish (search, result, &begin, &end, &wrapped, &error))
    {
      guint line;
      guint line_offset;

      line = gtk_text_iter_get_line (&begin);
      line_offset = gtk_text_iter_get_line_offset (&begin);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &begin, line, line_offset);

      line = gtk_text_iter_get_line (&end);
      line_offset = gtk_text_iter_get_line_offset (&end);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &end, line, line_offset);

      word = gtk_text_iter_get_slice (&begin, &end);
    }
  else
    {
      if (error != NULL)
        ide_task_return_error (task, g_steal_pointer (&error));
      else
        ide_task_return_boolean (task, TRUE);
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  self = ide_task_get_source_object (task);
  cancellable = ide_task_get_cancellable (task);
  state = ide_task_get_task_data (task);

  state->wrapped |= wrapped;

  if (state->wrapped)
    {
      GtkTextIter iter;

      gtk_text_buffer_get_iter_at_mark (buffer, &iter, state->mark);

      if (gtk_text_iter_compare (&begin, &iter) <= 0)
        {
          ide_task_return_boolean (task, TRUE);
          return;
        }
    }

  gbp_word_proposals_add (self, word);

  gtk_source_search_context_backward_async (search,
                                            &begin,
                                            cancellable,
                                            gbp_word_proposals_backward_cb,
                                            g_steal_pointer (&task));
}

void
gbp_word_proposals_populate_async (GbpWordProposals           *self,
                                   GtkSourceCompletionContext *context,
                                   GCancellable               *cancellable,
                                   GAsyncReadyCallback         callback,
                                   gpointer                    user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GtkSourceSearchContext) search = NULL;
  g_autoptr(GtkSourceSearchSettings) settings = NULL;
  g_autofree gchar *search_text = NULL;
  GtkTextBuffer *buffer;
  State *state;
  GtkTextIter begin, end;
  guint old_len;

  g_assert (GBP_IS_WORD_PROPOSALS (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_word_proposals_populate_async);

  old_len = self->items->len;

  g_clear_pointer (&self->last_word, g_free);

  if (old_len)
    {
      g_array_remove_range (self->items, 0, old_len);
      if (self->unfiltered->len > 0)
        g_ptr_array_remove_range (self->unfiltered, 0, self->unfiltered->len);
      g_hash_table_remove_all (self->words_dedup);
      g_string_chunk_clear (self->words);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, 0);
    }

  /*
   * We won't do anything if we don't have a word to complete. Otherwise
   * we'd just create a list of every word in the file. While that might
   * be interesting, it's more work than we want to do currently.
   */
  if (!gtk_source_completion_context_get_bounds (context, &begin, &end))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  self->last_word = gtk_text_iter_get_slice (&begin, &end);

  settings = gtk_source_search_settings_new ();
  gtk_source_search_settings_set_regex_enabled (settings, TRUE);
  gtk_source_search_settings_set_at_word_boundaries (settings, TRUE);
  search_text = g_strconcat (self->last_word, "[a-zA-Z0-9_]*", NULL);
  gtk_source_search_settings_set_search_text (settings, search_text);
  gtk_source_search_settings_set_wrap_around (settings, TRUE);

  buffer = GTK_TEXT_BUFFER (gtk_source_completion_context_get_buffer (context));
  search = gtk_source_search_context_new (GTK_SOURCE_BUFFER (buffer), settings);
  gtk_source_search_context_set_highlight (search, FALSE);

  state = g_slice_new0 (State);
  state->mark = gtk_text_buffer_create_mark (buffer, NULL, &begin, TRUE);
  g_object_ref (state->mark);
  ide_task_set_task_data (task, state, state_free);

  gtk_source_search_context_backward_async (search,
                                            &begin,
                                            NULL,
                                            gbp_word_proposals_backward_cb,
                                            g_steal_pointer (&task));
}

gboolean
gbp_word_proposals_populate_finish (GbpWordProposals  *self,
                                    GAsyncResult      *result,
                                    GError           **error)
{
  const gchar *word;
  guint old_len;

  g_return_val_if_fail (GBP_IS_WORD_PROPOSALS (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  if ((old_len = self->items->len))
    g_array_remove_range (self->items, 0, old_len);

  word = self->last_word ? self->last_word : "";

  for (guint i = 0; i < self->unfiltered->len; i++)
    {
      const gchar *element = g_ptr_array_index (self->unfiltered, i);
      guint priority;

      if (gtk_source_completion_fuzzy_match (element, word, &priority))
        {
          Item item = { element, priority };
          g_array_append_val (self->items, item);
        }
    }

  if (old_len || self->items->len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gint
compare_item (gconstpointer a,
              gconstpointer b)
{
  const Item *ai = a;
  const Item *bi = b;

  return (gint)ai->priority - (gint)bi->priority;
}

void
gbp_word_proposals_refilter (GbpWordProposals *self,
                             const gchar      *word)
{
  guint old_len = 0;

  g_return_if_fail (GBP_IS_WORD_PROPOSALS (self));

  if (word == NULL)
    word = "";

  if (g_strcmp0 (self->last_word, word) == 0)
    return;

  old_len = self->items->len;

  if (self->last_word && g_str_has_prefix (word, self->last_word))
    {
      for (guint i = self->items->len; i > 0; i--)
        {
          Item *item = &g_array_index (self->items, Item, i - 1);

          if (!gtk_source_completion_fuzzy_match (item->word, word, &item->priority))
            g_array_remove_index_fast (self->items, i - 1);
        }
    }
  else
    {
      if (old_len)
        g_array_remove_range (self->items, 0, old_len);

      for (guint i = 0; i < self->unfiltered->len; i++)
        {
          const gchar *element = g_ptr_array_index (self->unfiltered, i);
          guint priority;

          if (gtk_source_completion_fuzzy_match (element, word, &priority))
            {
              Item item = { element, priority };
              g_array_append_val (self->items, item);
            }
        }
    }

  g_array_sort (self->items, compare_item);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);

  g_free (self->last_word);
  self->last_word = g_strdup (word);
}

void
gbp_word_proposals_clear (GbpWordProposals *self)
{
  guint old_len;

  g_return_if_fail (GBP_IS_WORD_PROPOSALS (self));

  if ((old_len = self->items->len))
    g_array_remove_range (self->items, 0, old_len);

  if (self->unfiltered->len)
    g_ptr_array_remove_range (self->unfiltered, 0, self->unfiltered->len);

  g_hash_table_remove_all (self->words_dedup);
  g_string_chunk_clear (self->words);

  g_clear_pointer (&self->last_word, g_free);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, 0);
}

static GType
gbp_word_proposals_get_item_type (GListModel *model)
{
  return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gbp_word_proposals_get_n_items (GListModel *model)
{
  return GBP_WORD_PROPOSALS (model)->items->len;
}

static gpointer
gbp_word_proposals_get_item (GListModel *model,
                             guint       position)
{
  GbpWordProposals *self = (GbpWordProposals *)model;
  const Item *item;

  g_assert (GBP_IS_WORD_PROPOSALS (self));

  if (position >= self->items->len)
    return NULL;

  item = &g_array_index (self->items, Item, position);

  return gbp_word_proposal_new (item->word);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_word_proposals_get_n_items;
  iface->get_item_type = gbp_word_proposals_get_item_type;
  iface->get_item = gbp_word_proposals_get_item;
}
