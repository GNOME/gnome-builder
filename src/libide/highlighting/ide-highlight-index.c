/* ide-highlight-index.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "config.h"

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
  GVariant      *variant;
};

IdeHighlightIndex *
ide_highlight_index_new (void)
{
  IdeHighlightIndex *ret;

  ret = g_slice_new0 (IdeHighlightIndex);
  ret->ref_count = 1;
  ret->strings = g_string_chunk_new (ide_get_system_page_size ());
  ret->index = g_hash_table_new (g_str_hash, g_str_equal);

  DZL_COUNTER_INC (instances);

  return ret;
}

IdeHighlightIndex *
ide_highlight_index_new_from_variant (GVariant *variant)
{
  IdeHighlightIndex *self;

  self = ide_highlight_index_new ();

  if (variant != NULL)
    {
      g_autoptr(GVariant) unboxed = NULL;

      self->variant = g_variant_ref_sink (variant);

      if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
        variant = unboxed = g_variant_get_variant (variant);

      if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT))
        {
          GVariantIter iter;
          GVariant *value;
          gchar *key;

          g_variant_iter_init (&iter, variant);

          while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
            {
              if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING_ARRAY))
                {
                  g_autofree const gchar **strv = NULL;
                  const gchar *tag;
                  gsize len;

                  tag = g_string_chunk_insert (self->strings, key);
                  strv = g_variant_get_strv (value, &len);

                  for (gsize i = 0; i < len; i++)
                    {
                      const gchar *word = strv[i];

                      if (g_hash_table_contains (self->index, word))
                        continue;

                      /* word is guaranteed to be alive and valid inside our
                       * variant that we are storing. No need to add to the
                       * string chunk too.
                       */
                      g_hash_table_insert (self->index, (gchar *)word, (gchar *)tag);
                      self->count++;
                    }
                }
            }
        }
    }

  return self;
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

  g_clear_pointer (&self->strings, g_string_chunk_free);
  g_clear_pointer (&self->index, g_hash_table_unref);
  g_slice_free (IdeHighlightIndex, self);

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

/**
 * ide_highlight_index_to_variant:
 * @self: a #IdeHighlightIndex
 *
 * Creates a variant to represent the index. Useful to transport across IPC boundaries.
 *
 * Returns: (transfer full): a #GVariant
 */
GVariant *
ide_highlight_index_to_variant (IdeHighlightIndex *self)
{
  g_autoptr(GHashTable) arrays = NULL;
  GHashTableIter iter;
  const gchar *k, *v;
  GPtrArray *ar;
  GVariantDict dict;

  g_return_val_if_fail (self != NULL, NULL);

  arrays = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify)g_ptr_array_unref);

  g_hash_table_iter_init (&iter, self->index);
  while (g_hash_table_iter_next (&iter, (gpointer *)&k, (gpointer *)&v))
    {
      if G_UNLIKELY (!(ar = g_hash_table_lookup (arrays, v)))
        {
          ar = g_ptr_array_new ();
          g_hash_table_insert (arrays, (gchar *)v, ar);
        }

      g_ptr_array_add (ar, (gchar *)k);
    }

  g_variant_dict_init (&dict, NULL);

  g_hash_table_iter_init (&iter, arrays);
  while (g_hash_table_iter_next (&iter, (gpointer *)&k, (gpointer *)&ar))
    {
      GVariant *keys;

      g_ptr_array_add (ar, NULL);

      keys = g_variant_new_strv ((const gchar * const *)ar->pdata, ar->len - 1);
      g_variant_dict_insert_value (&dict, k, keys);
    }

  return g_variant_ref_sink (g_variant_dict_end (&dict));
}
