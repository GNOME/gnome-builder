/* ide-source-snippets.c
 *
 * Copyright © 2013 Christian Hergert <christian@hergert.me>
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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <string.h>

#include "snippets/ide-source-snippet.h"
#include "snippets/ide-source-snippet-chunk.h"
#include "snippets/ide-source-snippet-parser.h"
#include "snippets/ide-source-snippets.h"

struct _IdeSourceSnippets
{
  GObject  parent_instance;

  DzlTrie *snippets;
};


G_DEFINE_TYPE (IdeSourceSnippets, ide_source_snippets, G_TYPE_OBJECT)


IdeSourceSnippets *
ide_source_snippets_new (void)
{
  return g_object_new (IDE_TYPE_SOURCE_SNIPPETS,
                       NULL);
}

void
ide_source_snippets_clear (IdeSourceSnippets *snippets)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS (snippets));

  dzl_trie_destroy (snippets->snippets);
  snippets->snippets = dzl_trie_new (g_object_unref);
}

static gboolean
copy_into (DzlTrie     *trie,
           const gchar *key,
           gpointer     value,
           gpointer     user_data)
{
  IdeSourceSnippet *snippet = value;
  DzlTrie *dest = user_data;

  g_assert (dest);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));

  dzl_trie_insert (dest, key, g_object_ref (snippet));

  return FALSE;
}

void
ide_source_snippets_merge (IdeSourceSnippets *snippets,
                           IdeSourceSnippets *other)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS (other));

  dzl_trie_traverse (other->snippets,
                     "",
                     G_PRE_ORDER,
                     G_TRAVERSE_LEAVES,
                     -1,
                     copy_into,
                     snippets->snippets);
}

void
ide_source_snippets_add (IdeSourceSnippets *snippets,
                        IdeSourceSnippet  *snippet)
{
  const gchar *trigger;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (snippet));

  trigger = ide_source_snippet_get_trigger (snippet);
  dzl_trie_insert (snippets->snippets, trigger, g_object_ref (snippet));
}

static gboolean
ide_source_snippets_foreach_cb (DzlTrie     *trie,
                                const gchar *key,
                                gpointer     value,
                                gpointer     user_data)
{
  gpointer *closure = user_data;

  ((GFunc) closure[0])(value, closure[1]);

  return FALSE;
}

/**
 * ide_source_snippets_foreach:
 * @foreach_func: (scope call): A callback to execute for each snippet.
 *
 */
void
ide_source_snippets_foreach (IdeSourceSnippets *snippets,
                             const gchar       *prefix,
                             GFunc              foreach_func,
                             gpointer           user_data)
{
  gpointer closure[2] = { foreach_func, user_data };

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (foreach_func);

  if (!prefix)
    prefix = "";

  dzl_trie_traverse (snippets->snippets,
                     prefix,
                     G_PRE_ORDER,
                     G_TRAVERSE_LEAVES,
                     -1,
                     ide_source_snippets_foreach_cb,
                     (gpointer) closure);
}

static void
ide_source_snippets_finalize (GObject *object)
{
  IdeSourceSnippets *self = IDE_SOURCE_SNIPPETS (object);

  g_clear_pointer (&self->snippets, (GDestroyNotify) dzl_trie_unref);

  G_OBJECT_CLASS (ide_source_snippets_parent_class)->finalize (object);
}

static void
ide_source_snippets_class_init (IdeSourceSnippetsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = ide_source_snippets_finalize;
}

static void
ide_source_snippets_init (IdeSourceSnippets *snippets)
{
  snippets->snippets = dzl_trie_new (g_object_unref);
}

static gboolean
increment_count (DzlTrie     *trie,
                 const gchar *key,
                 gpointer     value,
                 gpointer     user_data)
{
  guint *count = user_data;
  (*count)++;
  return FALSE;
}

guint
ide_source_snippets_count (IdeSourceSnippets *self)
{
  guint count = 0;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPETS (self), 0);

  dzl_trie_traverse (self->snippets,
                     "",
                     G_PRE_ORDER,
                     G_TRAVERSE_LEAVES,
                     -1,
                     increment_count,
                     &count);

  return count;
}
