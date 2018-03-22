/* ide-completion-item.c
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

#define G_LOG_DOMAIN "ide-completion-item"

#include "config.h"

#include <string.h>

#include "sourceview/ide-completion-item.h"

G_DEFINE_ABSTRACT_TYPE (IdeCompletionItem, ide_completion_item, G_TYPE_OBJECT)

static gboolean
ide_completion_item_real_match (IdeCompletionItem *self,
                                const gchar       *query,
                                const gchar       *casefold)
{
  gboolean ret = FALSE;

  g_assert (IDE_IS_COMPLETION_ITEM (self));
  g_assert (query != NULL);
  g_assert (casefold != NULL);

  if (GTK_SOURCE_IS_COMPLETION_PROPOSAL (self))
    {
      gchar *text;

      text = gtk_source_completion_proposal_get_label (GTK_SOURCE_COMPLETION_PROPOSAL (self));
      ret = !!strstr (text ?: "", query);
      g_free (text);
    }

  return ret;
}

void
ide_completion_item_set_priority (IdeCompletionItem *self,
                                  guint              priority)
{
  g_return_if_fail (IDE_IS_COMPLETION_ITEM (self));

  self->priority = priority;
}

gboolean
ide_completion_item_match (IdeCompletionItem *self,
                           const gchar       *query,
                           const gchar       *casefold)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_ITEM (self), FALSE);

  return IDE_COMPLETION_ITEM_GET_CLASS (self)->match (self, query, casefold);
}

static void
ide_completion_item_class_init (IdeCompletionItemClass *klass)
{
  klass->match = ide_completion_item_real_match;
}

static void
ide_completion_item_init (IdeCompletionItem *self)
{
  self->link.data = self;
}

/**
 * ide_completion_item_fuzzy_match:
 * @haystack: the string to be searched.
 * @casefold_needle: A g_utf8_casefold() version of the needle.
 * @priority: (out) (allow-none): An optional location for the score of the match
 *
 * This helper function can do a fuzzy match for you giving a haystack and
 * casefolded needle. Casefold your needle using g_utf8_casefold() before
 * running the query against a batch of #IdeCompletionItem for the best performance.
 *
 * score will be set with the score of the match upon success. Otherwise,
 * it will be set to zero.
 *
 * Returns: %TRUE if @haystack matched @casefold_needle, otherwise %FALSE.
 */
gboolean
ide_completion_item_fuzzy_match (const gchar *haystack,
                                 const gchar *casefold_needle,
                                 guint       *priority)
{
  gint real_score = 0;

  for (; *casefold_needle; casefold_needle = g_utf8_next_char (casefold_needle))
    {
      gunichar ch = g_utf8_get_char (casefold_needle);
      const gchar *tmp;

      /*
       * Note that the following code is not really correct. We want
       * to be relatively fast here, but we also don't want to convert
       * strings to casefolded versions for querying on each compare.
       * So we use the casefold version and compare with upper. This
       * works relatively well since we are usually dealing with ASCII
       * for function names and symbols.
       */

      tmp = strchr (haystack, ch);

      if (tmp == NULL)
        {
          tmp = strchr (haystack, g_unichar_toupper (ch));
          if (tmp == NULL)
            return FALSE;
        }

      /*
       * Here we calculate the cost of this character into the score.
       * If we matched exactly on the next character, the cost is ZERO.
       * However, if we had to skip some characters, we have a cost
       * of 2*distance to the character. This is necessary so that
       * when we add the cost of the remaining haystack, strings which
       * exhausted @casefold_needle score lower (higher priority) than
       * strings which had to skip characters but matched the same
       * number of characters in the string.
       */
      real_score += (tmp - haystack) * 2;

      /*
       * Now move past our matching character so we cannot match
       * it a second time.
       */
      haystack = tmp + 1;
    }

  if (priority != NULL)
    *priority = real_score + strlen (haystack);

  return TRUE;
}

gchar *
ide_completion_item_fuzzy_highlight (const gchar *str,
                                     const gchar *match)
{
  static const gchar *begin = "<b>";
  static const gchar *end = "</b>";
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;
  gboolean element_open = FALSE;

  if (str == NULL || match == NULL)
    return g_strdup (str);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if ((str_ch == match_ch) || (g_unichar_tolower (str_ch) == g_unichar_tolower (match_ch)))
        {
          if (!element_open)
            {
              g_string_append (ret, begin);
              element_open = TRUE;
            }

          g_string_append_unichar (ret, str_ch);

          /* TODO: We could seek to the next char and append in a batch. */
          match = g_utf8_next_char (match);
        }
      else
        {
          if (element_open)
            {
              g_string_append (ret, end);
              element_open = FALSE;
            }

          g_string_append_unichar (ret, str_ch);
        }
    }

  if (element_open)
    g_string_append (ret, end);

  return g_string_free (ret, FALSE);
}
