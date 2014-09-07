/* trie.h
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRIE_H
#define TRIE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _Trie Trie;

typedef gboolean (*TrieTraverseFunc) (Trie        *trie,
                                      const gchar *key,
                                      gpointer     value,
                                      gpointer     user_data);

void      trie_destroy  (Trie             *trie);
void      trie_insert   (Trie             *trie,
                         const gchar      *key,
                         gpointer          value);
gpointer  trie_lookup   (Trie             *trie,
                         const gchar      *key);
Trie     *trie_new      (GDestroyNotify    value_destroy);
gboolean  trie_remove   (Trie             *trie,
                         const gchar      *key);
void      trie_traverse (Trie             *trie,
                         const gchar      *key,
                         GTraverseType     order,
                         GTraverseFlags    flags,
                         gint              max_depth,
                         TrieTraverseFunc  func,
                         gpointer          user_data);

G_END_DECLS

#endif /* TRIE_H */
