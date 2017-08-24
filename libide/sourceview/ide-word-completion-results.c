/* ide-word-completion-results.c
 *
 * Copyright (C) 2017 Umang Jain <mailumangjain@gmail.com>
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

#define G_LOG_DOMAIN "ide-word-completion-results"

#include "sourceview/ide-word-completion-results.h"
#include "sourceview/ide-word-completion-item.h"

struct _IdeWordCompletionResults
{
  IdeCompletionResults parent_instance;
};

G_DEFINE_TYPE (IdeWordCompletionResults,
               ide_word_completion_results,
               IDE_TYPE_COMPLETION_RESULTS)

static gint
ide_word_completion_results_compare (IdeCompletionResults *results,
                                     IdeCompletionItem    *left,
                                     IdeCompletionItem    *right)
{
  IdeWordCompletionItem *p1 = (IdeWordCompletionItem *) left;
  IdeWordCompletionItem *p2 = (IdeWordCompletionItem *) right;

  if (ide_word_completion_item_get_offset (p1) <
      ide_word_completion_item_get_offset (p2))
    return -1;
  else
    return 1;
}

static void
ide_word_completion_results_init (IdeWordCompletionResults *self)
{
}

static void
ide_word_completion_results_class_init (IdeWordCompletionResultsClass *klass)
{
  IdeCompletionResultsClass *results_class = IDE_COMPLETION_RESULTS_CLASS (klass);

  results_class->compare = ide_word_completion_results_compare;
}

IdeWordCompletionResults *
ide_word_completion_results_new (const gchar *query)
{
  return g_object_new (IDE_TYPE_WORD_COMPLETION_RESULTS,
                       "query", query,
                       NULL);
}
