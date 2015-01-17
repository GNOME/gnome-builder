/* gb-search-reducer.c
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

#include "gb-search-context.h"
#include "gb-search-provider.h"
#include "gb-search-reducer.h"
#include "gb-search-result.h"

void
gb_search_reducer_init (GbSearchReducer  *reducer,
                        GbSearchContext  *context,
                        GbSearchProvider *provider)
{
  g_return_if_fail (reducer);
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  reducer->context = context;
  reducer->provider = provider;
  reducer->sequence = g_sequence_new (g_object_unref);
  reducer->max_results = 15;
  reducer->count = 0;
}

void
gb_search_reducer_destroy (GbSearchReducer *reducer)
{
  g_return_if_fail (reducer);

  g_sequence_free (reducer->sequence);
}

void
gb_search_reducer_push (GbSearchReducer *reducer,
                        GbSearchResult  *result)
{
  g_return_if_fail (reducer);
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));

  if (reducer->max_results <= g_sequence_get_length (reducer->sequence))
    {
      GSequenceIter *iter;
      GbSearchResult *lowest;

      /* Remove lowest score */
      iter = g_sequence_get_begin_iter (reducer->sequence);
      lowest = g_sequence_get (iter);
      gb_search_context_remove_result (reducer->context, reducer->provider,
                                       lowest);
      g_sequence_remove (iter);
    }

  g_sequence_insert_sorted (reducer->sequence,
                            g_object_ref (result),
                            (GCompareDataFunc)gb_search_result_compare,
                            NULL);
  gb_search_context_add_result (reducer->context, reducer->provider, result);
}

gboolean
gb_search_reducer_accepts (GbSearchReducer *reducer,
                           gfloat           score)
{
  GSequenceIter *iter;

  g_return_val_if_fail (reducer, FALSE);

  if (g_sequence_get_length (reducer->sequence) < reducer->max_results)
    return TRUE;

  iter = g_sequence_get_begin_iter (reducer->sequence);

  if (iter)
    {
      GbSearchResult *result;

      result = g_sequence_get (iter);
      if (result)
        return score > gb_search_result_get_score (result);
    }

  return FALSE;
}
