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

  gint                 sort_direction;
};

G_DEFINE_TYPE (IdeWordCompletionResults,
               ide_word_completion_results,
               IDE_TYPE_COMPLETION_RESULTS)

enum
{
  PROP_0,
  PROP_SORT_DIRECTION,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static gint
ide_word_completion_results_compare (IdeCompletionResults *results,
                                     IdeCompletionItem    *left,
                                     IdeCompletionItem    *right)
{
  IdeWordCompletionResults *self = IDE_WORD_COMPLETION_RESULTS (results);
  IdeWordCompletionItem *p1 = (IdeWordCompletionItem *) left;
  IdeWordCompletionItem *p2 = (IdeWordCompletionItem *) right;
  gint offset_p1 = ide_word_completion_item_get_offset (p1);
  gint offset_p2 = ide_word_completion_item_get_offset (p2);
  gint rel_offset_p1= 0;
  gint rel_offset_p2 = 0;
  gint insert_offset = 0;
  gint end_offset = 0;

  insert_offset = ide_completion_results_get_insert_offset (results);
  end_offset = ide_completion_results_get_end_offset (results);

  g_assert (self->sort_direction != 0);
  g_assert (insert_offset != 0);
  g_assert (end_offset != 0);

  if (self->sort_direction == 1)
    {
      rel_offset_p1 = offset_p1 - insert_offset;

      /* Scan must had wrapped so relative offset < 0 possible */
      if (rel_offset_p1 < 0)
        rel_offset_p1 = end_offset - insert_offset + offset_p1;
        g_assert (rel_offset_p1 >= 0);

      rel_offset_p2 =  offset_p2 - insert_offset;

      if (rel_offset_p2 < 0)
        rel_offset_p2 = end_offset - insert_offset +  offset_p2;
        g_assert (rel_offset_p2 >= 0);

      if (rel_offset_p1 < rel_offset_p2)
        return -1;
      else
        return 1;
    }
  else
    {
      rel_offset_p1 = insert_offset - offset_p1;

      /* Scan must had wrapped so relative offset < 0 possible */
      if (rel_offset_p1 < 0)
        rel_offset_p1 = end_offset - offset_p1 + insert_offset;
      g_assert (rel_offset_p1 >= 0);

      rel_offset_p2 = insert_offset - offset_p2;

      if (rel_offset_p2 < 0)
        rel_offset_p2 = end_offset - offset_p2 + insert_offset;
      g_assert (rel_offset_p2 >= 0);

      if (rel_offset_p1 > rel_offset_p2)
        return 1;
      else
        return -1;
    }
}

static void
ide_word_completion_results_init (IdeWordCompletionResults *self)
{
}

static void
ide_word_completion_results_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeWordCompletionResults *self = IDE_WORD_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_SORT_DIRECTION:
      g_value_set_int (value, self->sort_direction);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_word_completion_results_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeWordCompletionResults *self = IDE_WORD_COMPLETION_RESULTS (object);

  switch (prop_id)
    {
    case PROP_SORT_DIRECTION:
      self->sort_direction = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_word_completion_results_class_init (IdeWordCompletionResultsClass *klass)
{
  IdeCompletionResultsClass *results_class = IDE_COMPLETION_RESULTS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_word_completion_results_get_property;
  object_class->set_property = ide_word_completion_results_set_property;
  results_class->compare = ide_word_completion_results_compare;

  properties [PROP_SORT_DIRECTION] =
    g_param_spec_int ("sort-direction",
                      "Sort direction",
                      "Determines whether to sort with ascending or descending value of offset",
                      -1,
                       1,
                       0,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT |  G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

IdeWordCompletionResults *
ide_word_completion_results_new (const gchar *query,
                                 gint         sort_direction)
{
  return g_object_new (IDE_TYPE_WORD_COMPLETION_RESULTS,
                       "query", query,
                       "sort-direction", sort_direction,
                       NULL);
}
