/* ide-completion-results.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-completion-results"

#include <dazzle.h>
#include <string.h>

#include "ide-debug.h"

#include "sourceview/ide-completion-results.h"
#include "util/ide-list-inline.h"

typedef struct
{
  /*
   * needs_refilter indicates that the result set must have
   * the linked list rebuilt from the array. Doing so must
   * have match() called on each item to determine its
   * visibility.
   */
  guint needs_refilter : 1;
  /*
   * If a g_list_sort() needs to be called on our synthesized
   * linked list of visible items.
   */
  guint needs_sort : 1;
  /*
   * If can_reuse_list is set, refilter requests may traverse
   * the linked list instead of a full array scan.
   */
  guint can_reuse_list : 1;
  /*
   * results contains all of our IdeCompletionItem results.
   * We use this array of items with embedded GList links to
   * create a zero-allocation linked list (well technically
   * the item is an allocation, but we can't get around that).
   */
  GPtrArray *results;
  /*
   * insert_offset and end_offset helps to determine new relative offsets
   * in case we can replay the results but in an another location. Hence,
   * recompute new offsets and sort. See ide_word_completion_results_compare
   * for details.
   */
  gint insert_offset;
  gint end_offset;
  /*
   * query is the filtering string that was used to create the
   * initial set of results. All future queries must have this
   * word as a prefix to be reusable.
   */
  gchar *query;
  /*
   * replay is the word that was replayed on the last call to
   * ide_completion_results_replay(). It allows us to continually
   * dive down in the result set without looking at all items.
   */
  gchar *replay;
  /*
   * As an optimization, the linked list for result nodes are
   * embedded in the IdeCompletionItem structures and we do not
   * allocate them. This is the pointer to the first item in the
   * result set that matches our query. It is not allocated
   * and do not try to free it or perform g_list_*() operations
   * upon it except for g_list_sort().
   */
  GList *head;
} IdeCompletionResultsPrivate;

typedef struct
{
  IdeCompletionResults *self;
  gint (*compare) (IdeCompletionResults *,
                   IdeCompletionItem *,
                   IdeCompletionItem *);
} SortState;

G_DEFINE_TYPE_WITH_PRIVATE (IdeCompletionResults, ide_completion_results, G_TYPE_OBJECT)

DZL_DEFINE_COUNTER (instances, "IdeCompletionResults", "Instances", "Number of IdeCompletionResults")

#define GET_ITEM(i) ((IdeCompletionItem *)(g_ptr_array_index((priv)->results, (i))))
#define GET_ITEM_LINK(item) (&((IdeCompletionItem *)(item))->link)

enum {
  PROP_0,
  PROP_QUERY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeCompletionResults *
ide_completion_results_new (const gchar *query)
{
  return g_object_new (IDE_TYPE_COMPLETION_RESULTS,
                       "query", query,
                       NULL);
}

/**
 * ide_completion_results_take_proposal:
 * @proposal: (transfer full): The completion item
 */
void
ide_completion_results_take_proposal (IdeCompletionResults *self,
                                      IdeCompletionItem    *item)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_if_fail (IDE_IS_COMPLETION_RESULTS (self));
  g_return_if_fail (IDE_IS_COMPLETION_ITEM (item));

  g_ptr_array_add (priv->results, item);

  priv->needs_refilter = TRUE;
  priv->needs_sort = TRUE;
  priv->can_reuse_list = FALSE;
}

static void
ide_completion_results_finalize (GObject *object)
{
  IdeCompletionResults *self = (IdeCompletionResults *)object;
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_clear_pointer (&priv->query, g_free);
  g_clear_pointer (&priv->replay, g_free);
  g_clear_pointer (&priv->results, g_ptr_array_unref);
  priv->head = NULL;

  G_OBJECT_CLASS (ide_completion_results_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}

const gchar *
ide_completion_results_get_query (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_COMPLETION_RESULTS (self), NULL);

  return priv->query;
}

static void
ide_completion_results_set_query (IdeCompletionResults *self,
                                  const gchar          *query)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_if_fail (IDE_IS_COMPLETION_RESULTS (self));
  g_return_if_fail (priv->query == NULL);

  if (query == NULL)
    query = "";

  priv->query = g_strdup (query);
  priv->replay = g_strdup (query);
  priv->can_reuse_list = FALSE;
  priv->needs_refilter = TRUE;
  priv->needs_sort = TRUE;
}

gboolean
ide_completion_results_replay (IdeCompletionResults *self,
                               const gchar          *query)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_COMPLETION_RESULTS (self), FALSE);
  g_return_val_if_fail (priv->query != NULL, FALSE);
  g_return_val_if_fail (query != NULL, FALSE);

  IDE_TRACE_MSG ("Checking if we can reply results: query=%s, last_query=%s", query, priv->query);

  if (g_str_has_prefix (query, priv->query))
    {
      const gchar *suffix = query + strlen (priv->query);

      /*
       * Only allow completing using this result set if we have characters
       * that could continue a function name, etc. In all the languages we
       * support this is alpha-numeric only. We could potentially turn this
       * into a vfunc if we need to support something other than that.
       */
      for (; *suffix; suffix = g_utf8_next_char (suffix))
        {
          gunichar ch = g_utf8_get_char (suffix);
          if (G_LIKELY (ch == '_' || g_unichar_isalnum (ch)))
            continue;
          IDE_RETURN (FALSE);
        }

      priv->can_reuse_list = (priv->replay != NULL && g_str_has_prefix (query, priv->replay));
      priv->needs_refilter = TRUE;
      priv->needs_sort = TRUE;

      g_free (priv->replay);
      priv->replay = g_strdup (query);

      IDE_RETURN (TRUE);
    }

  IDE_RETURN (FALSE);
}

static void
ide_completion_results_update_links (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);
  IdeCompletionItem *item;
  IdeCompletionItem *next;
  IdeCompletionItem *prev;
  guint i;

  g_assert (IDE_IS_COMPLETION_RESULTS (self));
  g_assert (priv->results != NULL);

  if (G_UNLIKELY (priv->results->len == 0))
    {
      priv->head = NULL;
      return;
    }

  /* Unrolling loops for the gentoo crowd */

  item = GET_ITEM (0);
  GET_ITEM_LINK (item)->prev = NULL;
  GET_ITEM_LINK (item)->next = (priv->results->len == 1)
                             ? NULL
                             : GET_ITEM_LINK (GET_ITEM (1));

  priv->head = GET_ITEM_LINK (item);

  prev = item;

  for (i = 1; i < (priv->results->len - 1); i++)
    {
      item = GET_ITEM (i);
      next = GET_ITEM (i + 1);

      GET_ITEM_LINK (item)->prev = GET_ITEM_LINK (prev);
      GET_ITEM_LINK (item)->next = GET_ITEM_LINK (next);

      prev = item;
    }

  if (priv->results->len > 1)
    {
      item = GET_ITEM (priv->results->len - 1);
      GET_ITEM_LINK (item)->prev = GET_ITEM_LINK (GET_ITEM (priv->results->len - 2));
      GET_ITEM_LINK (item)->next = NULL;
    }
}

static void
ide_completion_results_refilter (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);
  g_autofree gchar *casefold = NULL;

  g_assert (IDE_IS_COMPLETION_RESULTS (self));
  g_assert (priv->results != NULL);

  if (priv->query == NULL || priv->replay == NULL || priv->results->len == 0)
    return;

  /*
   * By traversing the linked list nodes instead of the array, we allow
   * ourselves to avoid rechecking items we already know filtered.
   * We do need to be mindful of this in case the user backspaced
   * and our list is no longer a continual "deep dive" of matched items.
   */
  if (G_UNLIKELY (!priv->can_reuse_list))
    ide_completion_results_update_links (self);

  casefold = g_utf8_casefold (priv->replay, -1);

  if (G_UNLIKELY (!g_str_is_ascii (casefold)))
    {
      g_warning ("Item filtering requires ascii input.");
      return;
    }

  for (GList *iter = priv->head; iter; iter = iter->next)
    {
      IdeCompletionItem *item = iter->data;

      if (!IDE_COMPLETION_ITEM_GET_CLASS (item)->match (item, priv->replay, casefold))
        {
          if (iter->prev != NULL)
            iter->prev->next = iter->next;
          else
            priv->head = iter->next;

          if (iter->next != NULL)
            iter->next->prev = iter->prev;
        }
    }
}

static gint
compare_fast (const IdeCompletionItem *left,
              const IdeCompletionItem *right)
{
  if (left->priority < right->priority)
    return -1;
  else if (left->priority > right->priority)
    return 1;
  else
    return 0;
}


static gint
sort_state_compare (gconstpointer a,
                    gconstpointer b,
                    gpointer      user_data)
{
  SortState *state = user_data;

  return state->compare (state->self, (IdeCompletionItem *)a, (IdeCompletionItem *)b);
}

static void
ide_completion_results_resort (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);
  IdeCompletionResultsClass *klass = IDE_COMPLETION_RESULTS_GET_CLASS (self);
  SortState state;

  /*
   * Instead of invoking the vfunc for every item, save ourself an extra
   * dereference and call g_list_sort() directly with our compare funcs.
   */
  if (G_LIKELY (klass->compare == NULL))
    {
      priv->head = ide_list_sort (priv->head, (GCompareFunc)compare_fast);
      return;
    }

  state.self = self;
  state.compare = klass->compare;
  priv->head = ide_list_sort_with_data (priv->head, sort_state_compare, &state);
}

void
ide_completion_results_present (IdeCompletionResults        *self,
                                GtkSourceCompletionProvider *provider,
                                GtkSourceCompletionContext  *context)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);
  GtkTextIter insert_iter;
  GtkTextIter end_iter;

  g_return_if_fail (IDE_IS_COMPLETION_RESULTS (self));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
  g_return_if_fail (priv->query != NULL);
  g_return_if_fail (priv->replay != NULL);

  /* Maybe find some "if logic" block to detect that we are doing word-completion */
  gtk_source_completion_context_get_iter (context, &insert_iter);
  gtk_text_buffer_get_end_iter (gtk_text_iter_get_buffer (&insert_iter),
                                &end_iter);

  priv->insert_offset = gtk_text_iter_get_offset (&insert_iter);
  priv->end_offset = gtk_text_iter_get_offset (&end_iter);

  if (priv->needs_refilter)
    {
      ide_completion_results_refilter (self);
      priv->needs_refilter = FALSE;
    }

  if (priv->needs_sort)
    {
      ide_completion_results_resort (self);
      priv->needs_sort = FALSE;
    }

  gtk_source_completion_context_add_proposals (context, provider, priv->head, TRUE);
}

static void
ide_completion_results_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCompletionResults *self = IDE_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_QUERY:
      g_value_set_string (value, ide_completion_results_get_query (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_results_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeCompletionResults *self = IDE_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_QUERY:
      ide_completion_results_set_query (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_results_class_init (IdeCompletionResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_completion_results_finalize;
  object_class->get_property = ide_completion_results_get_property;
  object_class->set_property = ide_completion_results_set_property;

  properties [PROP_QUERY] =
    g_param_spec_string ("query",
                         "Query",
                         "Query",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_completion_results_init (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  DZL_COUNTER_INC (instances);

  priv->results = g_ptr_array_new_with_free_func (g_object_unref);
  priv->head = NULL;
  priv->query = NULL;
}

guint
ide_completion_results_get_size (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_COMPLETION_RESULTS (self), 0);

  return priv->results != NULL ? priv->results->len : 0;
}

gint
ide_completion_results_get_insert_offset (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_COMPLETION_RESULTS (self), 0);

  return priv->insert_offset;
}

gint
ide_completion_results_get_end_offset (IdeCompletionResults *self)
{
  IdeCompletionResultsPrivate *priv = ide_completion_results_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_COMPLETION_RESULTS (self), 0);

  return priv->end_offset;
}
