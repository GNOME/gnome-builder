/* gb-source-snippets.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <string.h>

#include "gb-source-snippet.h"
#include "gb-source-snippet-chunk.h"
#include "gb-source-snippet-parser.h"
#include "gb-source-snippets.h"

#include "trie.h"

G_DEFINE_TYPE (GbSourceSnippets, gb_source_snippets, G_TYPE_OBJECT)

struct _GbSourceSnippetsPrivate
{
  Trie *snippets;
};

GbSourceSnippets *
gb_source_snippets_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_SNIPPETS,
                       NULL);
}

void
gb_source_snippets_clear (GbSourceSnippets *snippets)
{
  GbSourceSnippetsPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_SNIPPETS (snippets));

  priv = snippets->priv;

  trie_destroy (priv->snippets);
  priv->snippets = trie_new (g_object_unref);
}

static gboolean
copy_into (Trie        *trie,
           const gchar *key,
           gpointer     value,
           gpointer     user_data)
{
  GbSourceSnippet *snippet = value;
  Trie *dest = user_data;

  g_assert (dest);
  g_assert (GB_IS_SOURCE_SNIPPET (snippet));

  trie_insert (dest, key, g_object_ref (snippet));

  return FALSE;
}

void
gb_source_snippets_merge (GbSourceSnippets *snippets,
                          GbSourceSnippets *other)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (GB_IS_SOURCE_SNIPPETS (other));

  trie_traverse (other->priv->snippets,
                 "",
                 G_PRE_ORDER,
                 G_TRAVERSE_LEAVES,
                 -1,
                 copy_into,
                 snippets->priv->snippets);
}

void
gb_source_snippets_add (GbSourceSnippets *snippets,
                        GbSourceSnippet  *snippet)
{
  const gchar *trigger;

  g_return_if_fail (GB_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));

  trigger = gb_source_snippet_get_trigger (snippet);
  trie_insert (snippets->priv->snippets, trigger, g_object_ref (snippet));
}

gboolean
gb_source_snippets_foreach_cb (Trie        *trie,
                               const gchar *key,
                               gpointer     value,
                               gpointer     user_data)
{
  gpointer *closure = user_data;

  ((GFunc) closure[0])(value, closure[1]);

  return FALSE;
}

void
gb_source_snippets_foreach (GbSourceSnippets *snippets,
                            const gchar      *prefix,
                            GFunc             foreach_func,
                            gpointer          user_data)
{
  GbSourceSnippetsPrivate *priv;
  gpointer closure[2] = { foreach_func, user_data };

  g_return_if_fail (GB_IS_SOURCE_SNIPPETS (snippets));
  g_return_if_fail (foreach_func);

  priv = snippets->priv;

  if (!prefix)
    prefix = "";

  trie_traverse (priv->snippets,
                 prefix,
                 G_PRE_ORDER,
                 G_TRAVERSE_LEAVES,
                 -1,
                 gb_source_snippets_foreach_cb,
                 (gpointer) closure);
}

static void
gb_source_snippets_finalize (GObject *object)
{
  GbSourceSnippetsPrivate *priv;

  priv = GB_SOURCE_SNIPPETS (object)->priv;

  g_clear_pointer (&priv->snippets, (GDestroyNotify) trie_destroy);

  G_OBJECT_CLASS (gb_source_snippets_parent_class)->finalize (object);
}

static void
gb_source_snippets_class_init (GbSourceSnippetsClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_snippets_finalize;
  g_type_class_add_private (object_class, sizeof (GbSourceSnippetsPrivate));
}

static void
gb_source_snippets_init (GbSourceSnippets *snippets)
{
  snippets->priv = G_TYPE_INSTANCE_GET_PRIVATE (snippets,
                                                GB_TYPE_SOURCE_SNIPPETS,
                                                GbSourceSnippetsPrivate);

  snippets->priv->snippets = trie_new (g_object_unref);
}
