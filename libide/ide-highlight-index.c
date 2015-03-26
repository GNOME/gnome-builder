/* ide-highlight-index.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/user.h>

#include "ide-highlight-index.h"

struct _IdeHighlightIndex
{
  volatile gint  ref_count;
  GStringChunk  *strings;
  GHashTable    *index;
};

IdeHighlightIndex *
ide_highlight_index_new (void)
{
  IdeHighlightIndex *ret;

  ret = g_new0 (IdeHighlightIndex, 1);
  ret->ref_count = 1;
  ret->strings = g_string_chunk_new (PAGE_SIZE);
  ret->index = g_hash_table_new (g_str_hash, g_str_equal);

  return ret;
}

void
ide_highlight_index_insert (IdeHighlightIndex *self,
                            const gchar       *word,
                            IdeHighlightKind   kind)
{
  gchar *key;

  g_assert (self);
  g_assert (word);
  g_assert (kind != IDE_HIGHLIGHT_KIND_NONE);

  if (g_hash_table_contains (self->index, word))
    return;

  key = g_string_chunk_insert (self->strings, word);
  g_hash_table_insert (self->index, key, GINT_TO_POINTER (kind));
}

IdeHighlightKind
ide_highlight_index_lookup (IdeHighlightIndex *self,
                            const gchar       *word)
{
  gpointer value;

  g_assert (self);
  g_assert (word);

  value = g_hash_table_lookup (self->index, word);

  return GPOINTER_TO_INT (value);
}

IdeHighlightIndex *
ide_highlight_index_ref (IdeHighlightIndex *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, >, 0);

  return self;
}

void
ide_highlight_index_unref (IdeHighlightIndex *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, >, 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_string_chunk_free (self->strings);
      g_hash_table_unref (self->index);
      g_free (self);
    }
}
