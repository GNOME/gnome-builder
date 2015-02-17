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

#include "ide-search-context.h"
#include "ide-search-provider.h"
#include "ide-search-reducer.h"
#include "ide-search-result.h"

void
ide_search_reducer_init (IdeSearchReducer  *reducer,
                         IdeSearchContext  *context,
                         IdeSearchProvider *provider)
{
  g_return_if_fail (reducer);
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  reducer->context = context;
  reducer->provider = provider;
  reducer->sequence = g_sequence_new (g_object_unref);
  reducer->max_results = 10;
  reducer->count = 0;
}

void
ide_search_reducer_destroy (IdeSearchReducer *reducer)
{
  g_return_if_fail (reducer);

  if (reducer->sequence)
    g_sequence_free (reducer->sequence);
}

void
ide_search_reducer_push (IdeSearchReducer *reducer,
                         IdeSearchResult  *result)
{
  g_return_if_fail (reducer);
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  if (reducer->max_results <= g_sequence_get_length (reducer->sequence))
    {
      GSequenceIter *iter;
      IdeSearchResult *lowest;

      /* Remove lowest score */
      iter = g_sequence_get_begin_iter (reducer->sequence);
      lowest = g_sequence_get (iter);
      ide_search_context_remove_result (reducer->context, reducer->provider,
                                       lowest);
      g_sequence_remove (iter);
    }

  g_sequence_insert_sorted (reducer->sequence,
                            g_object_ref (result),
                            (GCompareDataFunc)ide_search_result_compare,
                            NULL);
  ide_search_context_add_result (reducer->context, reducer->provider, result);
}

gboolean
ide_search_reducer_accepts (IdeSearchReducer *reducer,
                            gfloat            score)
{
  GSequenceIter *iter;

  g_return_val_if_fail (reducer, FALSE);

  if (g_sequence_get_length (reducer->sequence) < reducer->max_results)
    return TRUE;

  iter = g_sequence_get_begin_iter (reducer->sequence);

  if (iter)
    {
      IdeSearchResult *result;

      result = g_sequence_get (iter);
      if (result)
        return score > ide_search_result_get_score (result);
    }

  return FALSE;
}
