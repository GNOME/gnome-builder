/* ide-highlight-index.c
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

#define G_LOG_DOMAIN "ide-highlight-index"

#include <dazzle.h>
#include <string.h>

#include "ide-debug.h"

#include "highlighting/ide-highlight-index.h"
#include "util/ide-posix.h"

G_DEFINE_BOXED_TYPE (IdeHighlightIndex, ide_highlight_index,
                     ide_highlight_index_ref, ide_highlight_index_unref)

DZL_DEFINE_COUNTER (instances, "IdeHighlightIndex", "Instances", "Number of indexes")

struct _IdeHighlightIndex
{
  volatile gint  ref_count;

  /* For debugging info */
  guint          count;
  gsize          chunk_size;

  GStringChunk  *strings;
  GHashTable    *index;
};

IdeHighlightIndex *
ide_highlight_index_new (void)
{
  IdeHighlightIndex *ret;

  ret = g_new0 (IdeHighlightIndex, 1);
  ret->ref_count = 1;
  ret->strings = g_string_chunk_new (ide_get_system_page_size ());
  ret->index = g_hash_table_new (g_str_hash, g_str_equal);

  DZL_COUNTER_INC (instances);

  return ret;
}

void
ide_highlight_index_insert (IdeHighlightIndex *self,
                            const gchar       *word,
                            gpointer           tag)
{
  gchar *key;

  g_assert (self);
  g_assert (tag != NULL);

  if (word == NULL || word[0] == '\0')
    return;

  if (g_hash_table_contains (self->index, word))
    return;

  self->count++;
  self->chunk_size += strlen (word) + 1;

  key = g_string_chunk_insert (self->strings, word);
  g_hash_table_insert (self->index, key, tag);
}

/**
 * ide_highlight_index_lookup:
 * @self: An #IdeHighlightIndex.
 *
 * Gets the pointer tag that was registered for @word, or %NULL.  This can be
 * any arbitrary value. Some highlight engines might use it to point at
 * internal structures or strings they know about to optimize later work.
 *
 * Returns: (transfer none) (nullable): Highlighter specific tag.
 */
gpointer
ide_highlight_index_lookup (IdeHighlightIndex *self,
                            const gchar       *word)
{
  g_assert (self);
  g_assert (word);

  return g_hash_table_lookup (self->index, word);
}

IdeHighlightIndex *
ide_highlight_index_ref (IdeHighlightIndex *self)
{
  g_assert (self);
  g_assert (self->ref_count > 0);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

static void
ide_highlight_index_finalize (IdeHighlightIndex *self)
{
  IDE_ENTRY;

  g_string_chunk_free (self->strings);
  g_hash_table_unref (self->index);
  g_free (self);

  DZL_COUNTER_DEC (instances);

  IDE_EXIT;
}

void
ide_highlight_index_unref (IdeHighlightIndex *self)
{
  g_assert (self);
  g_assert (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_highlight_index_finalize (self);
}

void
ide_highlight_index_dump (IdeHighlightIndex *self)
{
  g_autofree gchar *format = NULL;

  g_assert (self);

  format = g_format_size (self->chunk_size);
  g_debug ("IdeHighlightIndex (%p) contains %u items and consumes %s.",
           self, self->count, format);
}
