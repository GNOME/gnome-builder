/* ide-search-reducer.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-search-reducer"

#include "ide-search-reducer.h"
#include "ide-search-result.h"

#define DEFAULT_MAX_ITEMS 1000

void
ide_search_reducer_init (IdeSearchReducer  *reducer,
                         gsize              max_results)
{
  g_return_if_fail (reducer != NULL);

  reducer->sequence = g_sequence_new (g_object_unref);
  reducer->max_results = max_results ? max_results : DEFAULT_MAX_ITEMS;
  reducer->count = 0;
}

void
ide_search_reducer_destroy (IdeSearchReducer *reducer)
{
  g_return_if_fail (reducer != NULL);

  if (reducer->sequence != NULL)
    g_sequence_free (reducer->sequence);
}

GPtrArray *
ide_search_reducer_free (IdeSearchReducer *reducer,
                         gboolean          free_results)
{
  GPtrArray *ar;
  GSequenceIter *iter;
  GSequenceIter *end;

  g_return_val_if_fail (reducer != NULL, NULL);

  if (free_results)
    {
      ide_search_reducer_destroy (reducer);
      return NULL;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  end = g_sequence_get_end_iter (reducer->sequence);

  for (iter = g_sequence_get_begin_iter (reducer->sequence);
       iter != end;
       iter = g_sequence_iter_next (iter))
    {
      IdeSearchResult *result = g_sequence_get (iter);
      g_ptr_array_add (ar, g_object_ref (result));
    }

  g_sequence_free (reducer->sequence);

  reducer->sequence = NULL;
  reducer->max_results = 0;
  reducer->count = 0;

  return ar;
}

/**
 * ide_search_reducer_take:
 * @self: an #IdeSearchReducer
 * @result: (transfer full): an #IdeSearchResult
 *
 * Like ide_search_reducer_push() but takes ownership of @result by
 * stealing the reference.
 */
void
ide_search_reducer_take (IdeSearchReducer *reducer,
                         IdeSearchResult  *result)
{
  g_assert (reducer != NULL);
  g_assert (IDE_IS_SEARCH_RESULT (result));

  if (reducer->count == reducer->max_results)
    /* Remove lowest score */
    g_sequence_remove (g_sequence_get_begin_iter (reducer->sequence));
  else
    reducer->count++;

  g_sequence_insert_sorted (reducer->sequence,
                            result,
                            (GCompareDataFunc)ide_search_result_compare,
                            NULL);
}

void
ide_search_reducer_push (IdeSearchReducer *reducer,
                         IdeSearchResult  *result)
{
  g_assert (reducer != NULL);
  g_assert (IDE_IS_SEARCH_RESULT (result));

  ide_search_reducer_take (reducer, g_object_ref (result));
}

gboolean
ide_search_reducer_accepts (IdeSearchReducer *reducer,
                            gfloat            score)
{
  GSequenceIter *iter;

  g_return_val_if_fail (reducer, FALSE);

  if (reducer->count < reducer->max_results)
    return TRUE;

  iter = g_sequence_get_begin_iter (reducer->sequence);

  if (iter != NULL)
    {
      IdeSearchResult *result;

      result = g_sequence_get (iter);
      if (result)
        return score > ide_search_result_get_score (result);
    }

  return FALSE;
}
