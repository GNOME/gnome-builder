/* trie.c
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

#include <string.h>

#include <glib/gi18n.h>

#include "trie.h"

#if defined(__LP64__) || __WORDSIZE == 64
#define TRIE_64 1
#endif

#define STATIC_ASSERT(a)                              \
   G_STMT_START {                                     \
      G_GNUC_UNUSED gchar static_assert[(a)? 0 : -1]; \
   } G_STMT_END

/**
 * SECTION:trie
 * @title: Trie
 * @short_description: A generic prefix tree.
 *
 * The #Trie struct and its associated functions provide a Trie data structure,
 * where nodes in the tree can contain arbitrary data.
 *
 * To create a new #Trie use trie_new(). You can free it with trie_free().
 * To insert a key and value pair into the #Trie use trie_insert().
 * To remove a key from the #Trie use trie_remove().
 * To traverse all children of the #Trie from a given key use trie_traverse().
 */

typedef struct _TrieNode      TrieNode;
typedef struct _TrieNodeChunk TrieNodeChunk;

/**
 * Try to optimize for 64 bit vs 32 bit pointers. We are assuming that the
 * 32 bit also has a 32 byte cacheline. This is very likely not the case
 * everwhere, such as x86_64 compiled with -m32. However, accomidating that
 * will require some changes to the layout of each chunk.
 */
#ifdef TRIE_64
#define FIRST_CHUNK_KEYS         4
#define TRIE_NODE_SIZE          64
#define TRIE_NODE_CHUNK_SIZE    64
#define TRIE_NODE_CHUNK_KEYS(c) (((c)->is_inline) ? 4 : 6)
#else
#define FIRST_CHUNK_KEYS         3
#define TRIE_NODE_SIZE          32
#define TRIE_NODE_CHUNK_SIZE    32
#define TRIE_NODE_CHUNK_KEYS(c) (((c)->is_inline) ? 3 : 5)
#endif

/**
 * TrieNodeChunk:
 * @flags: Flags describing behaviors of the TrieNodeChunk.
 * @count: The number of items added to this chunk.
 * @keys: The keys for @children.
 * @next: The next #TrieNodeChunk if there is one.
 * @children: The children #TrieNodeChunk or %NULL. If the chunk is
 *   inline the TrieNode, then there will be fewer items.
 */
#pragma pack(push, 1)
struct _TrieNodeChunk
{
   TrieNodeChunk *next;
   gboolean       is_inline : 1;
   guint          flags : 7;
   guint          count : 8;
   guint8         keys[6];
   TrieNode      *children[0];
};
#pragma pack(pop)

/**
 * TrieNode:
 * @parent: The parent #TrieNode. When a node is destroyed, it may need to
 *    walk up to the parent node and unlink itself.
 * @value: A pointer to the user provided value, or %NULL.
 * @chunk: The first chunk in the chain. Inline chunks have fewer children
 *    elements than extra allocated chunks so that they are cache aligned.
 */
#pragma pack(push, 1)
struct _TrieNode
{
   TrieNode      *parent;
   gpointer       value;
   TrieNodeChunk  chunk;
};
#pragma pack(pop)

/**
 * Trie:
 * @value_destroy: A #GDestroyNotify to free data pointers.
 * @root: The root TrieNode.
 */
struct _Trie
{
   GDestroyNotify  value_destroy;
   TrieNode       *root;
};

/**
 * trie_malloc0:
 * @trie: A #Trie
 * @size: Number of bytes to allocate.
 *
 * Wrapper function to allocate a memory chunk. This gives us somewhere to
 * perform the abstraction if we want to use mmap()'d I/O at some point.
 * The memory will be zero'd before being returned.
 *
 * Returns: A pointer to the allocation.
 */
static gpointer
trie_malloc0 (Trie  *trie,
              gsize  size)
{
   return g_malloc0(size);
}

/**
 * trie_free:
 * @trie: A #Trie.
 * @data: The data to free.
 *
 * Frees a portion of memory allocated by @trie.
 */
static void
trie_free (Trie     *trie,
           gpointer  data)
{
   g_free(data);
}

/**
 * trie_node_new:
 * @trie: A #Trie.
 * @parent: The nodes parent or %NULL.
 *
 * Create a new node that can be placed in a Trie. The node contains a chunk
 * embedded in it that may contain only 4 pointers instead of the full 6 do
 * to the overhead of the TrieNode itself.
 *
 * Returns: A newly allocated TrieNode that should be freed with g_free().
 */
TrieNode *
trie_node_new (Trie     *trie,
               TrieNode *parent)
{
   TrieNode *node;

   node = trie_malloc0(trie, TRIE_NODE_SIZE);
   node->chunk.is_inline = TRUE;
   node->parent = parent;
   return node;
}

/**
 * trie_node_chunk_is_full:
 * @chunk: A #TrieNodeChunk.
 *
 * Checks to see if all children slots are full in @chunk.
 *
 * Returns: %TRUE if there are no free slots in @chunk.
 */
G_INLINE_FUNC gboolean
trie_node_chunk_is_full (TrieNodeChunk *chunk)
{
   g_assert(chunk);
   return (chunk->count == TRIE_NODE_CHUNK_KEYS(chunk));
}

/**
 * trie_node_chunk_new:
 * @trie: The Trie that the chunk belongs to.
 *
 * Creates a new #TrieNodeChunk with empty state.
 *
 * Returns: (transfer full): A #TrieNodeChunk.
 */
TrieNodeChunk *
trie_node_chunk_new (Trie *trie)
{
   return trie_malloc0(trie, TRIE_NODE_CHUNK_SIZE);
}

/**
 * trie_append_to_node:
 * @chunk: A #TrieNodeChunk.
 * @key: The key to append.
 * @child: A #TrieNode to append.
 *
 * Appends @child to the chunk. If there is not room in the chunk,
 * then a new chunk will be added and append to @chunk.
 *
 * @chunk MUST be the last chunk in the chain (therefore chunk->next
 * is %NULL).
 */
static void
trie_append_to_node (Trie          *trie,
                     TrieNode      *node,
                     TrieNodeChunk *chunk,
                     guint8         key,
                     TrieNode      *child)
{
   g_assert(trie);
   g_assert(node);
   g_assert(chunk);
   g_assert(child);

   if (trie_node_chunk_is_full(chunk)) {
      chunk->next = trie_node_chunk_new(trie);
      chunk = chunk->next;
   }

   chunk->keys[chunk->count] = key;
   chunk->children[chunk->count] = child;
   chunk->count++;
}

/**
 * trie_node_move_to_front:
 * @node: A #TrieNode.
 * @chunk: A #TrieNodeChunk.
 * @idx: The index of the item to move.
 *
 * Moves the key and child found at @idx to the beginning of the first chunk
 * to achieve better cache locality.
 */
static void
trie_node_move_to_front (TrieNode      *node,
                         TrieNodeChunk *chunk,
                         guint          idx)
{
   TrieNodeChunk *first;
   TrieNode *child;
   guint8 offset;
   guint8 key;

   g_assert(node);
   g_assert(chunk);

   first = &node->chunk;

   key = chunk->keys[idx];
   child = chunk->children[idx];

   offset = ((first == chunk) ? first->count : FIRST_CHUNK_KEYS) - 1;
   chunk->keys[idx] = first->keys[offset];
   chunk->children[idx] = first->children[offset];

#ifdef TRIE_64
   STATIC_ASSERT(sizeof(gpointer) == 8);
   STATIC_ASSERT((FIRST_CHUNK_KEYS-1) == 3);
   STATIC_ASSERT(((FIRST_CHUNK_KEYS-1) * sizeof(gpointer)) == 24);
#else
   STATIC_ASSERT(sizeof(gpointer) == 4);
   STATIC_ASSERT((FIRST_CHUNK_KEYS-1) == 2);
   STATIC_ASSERT(((FIRST_CHUNK_KEYS-1) * sizeof(gpointer)) == 8);
#endif

   memmove(&first->keys[1], &first->keys[0], (FIRST_CHUNK_KEYS-1));
   memmove(&first->children[1], &first->children[0], (FIRST_CHUNK_KEYS-1) * sizeof(TrieNode *));

   first->keys[0] = key;
   first->children[0] = child;
}

/**
 * trie_find_node:
 * @trie: The #Trie we are searching.
 * @node: A #TrieNode.
 * @key: The key to find in this node.
 *
 * Searches the chunk chain of the current node for the key provided. If
 * found, the child node for that key is returned.
 *
 * Returns: (transfer none): A #TrieNode or %NULL.
 */
static TrieNode *
trie_find_node (Trie     *trie,
                TrieNode *node,
                guint8    key)
{
   TrieNodeChunk *iter;
   guint i;

   g_assert(node);

   for (iter = &node->chunk; iter; iter = iter->next) {
      for (i = 0; i < iter->count; i++) {
         if (iter->keys[i] == key) {
            if (iter != &node->chunk) {
               trie_node_move_to_front(node, iter, i);
               __builtin_prefetch(node->chunk.children[0]);
               return node->chunk.children[0];
            }
            __builtin_prefetch(iter->children[i]);
            return iter->children[i];
         }
      }
   }

   return NULL;
}

/**
 * trie_find_or_create_node:
 * @trie: A #Trie.
 * @node: A #TrieNode.
 * @key: The key to insert.
 *
 * Attempts to find key within @node. If @key is not found, it is added to the
 * node. The child for the key is returned.
 *
 * Returns: (transfer none): The child #TrieNode for @key.
 */
static TrieNode *
trie_find_or_create_node (Trie     *trie,
                          TrieNode *node,
                          guint8    key)
{
   TrieNodeChunk *iter;
   TrieNodeChunk *last = NULL;
   guint i;

   g_assert(node);

   for (iter = &node->chunk; iter; iter = iter->next) {
      for (i = 0; i < iter->count; i++) {
         if (iter->keys[i] == key) {
            if (iter != &node->chunk) {
               trie_node_move_to_front(node, iter, i);
               __builtin_prefetch(node->chunk.children[0]);
               return node->chunk.children[0];
            }
            __builtin_prefetch(iter->children[i]);
            return iter->children[i];
         }
      }
      last = iter;
   }

   g_assert(last);

   node = trie_node_new(trie, node);
   trie_append_to_node(trie, node->parent, last, key, node);
   return node;
}

/**
 * trie_node_remove_fast:
 * @node: A #TrieNode.
 * @chunk: A #TrieNodeChunk.
 * @idx: The child within the chunk.
 *
 * Removes child at index @idx from the chunk. The last item in the
 * chain of chunks will be moved to the slot indicated by @idx.
 */
G_INLINE_FUNC void
trie_node_remove_fast (TrieNode      *node,
                       TrieNodeChunk *chunk,
                       guint          idx)
{
   TrieNodeChunk *iter;

   g_assert(node);
   g_assert(chunk);

   for (iter = chunk; iter->next && iter->next->count; iter = iter->next) { }

   g_assert(iter->count);

   chunk->keys[idx] = iter->keys[iter->count-1];
   chunk->children[idx] = iter->children[iter->count-1];

   iter->count--;

   iter->keys[iter->count] = '\0';
   iter->children[iter->count] = NULL;
}

/**
 * trie_node_unlink:
 * @node: A #TrieNode.
 *
 * Unlinks @node from the Trie. The parent node has it's pointer to @node
 * removed.
 */
static void
trie_node_unlink (TrieNode *node)
{
   TrieNodeChunk *iter;
   TrieNode *parent;
   guint i;

   g_assert(node);

   if ((parent = node->parent)) {
      node->parent = NULL;
      for (iter = &parent->chunk; iter; iter = iter->next) {
         for (i = 0; i < iter->count; i++) {
            if (iter->children[i] == node) {
               trie_node_remove_fast(node, iter, i);
               g_assert(iter->children[i] != node);
               return;
            }
         }
      }
   }
}

/**
 * trie_destroy_node:
 * @trie: A #Trie.
 * @node: A #TrieNode.
 * @value_destroy: A #GDestroyNotify or %NULL.
 *
 * Removes @node from the #Trie and releases all memory associated with it.
 * If the nodes value is set, @value_destroy will be called to release it.
 *
 * The reclaimation happens as such:
 *
 * 1) the node is unlinked from its parent.
 * 2) each of the children are destroyed, leaving us an empty chain.
 * 3) each of the allocated chain links are freed.
 * 4) the value pointer is freed.
 * 5) the structure itself is freed.
 */
static void
trie_destroy_node (Trie           *trie,
                   TrieNode       *node,
                   GDestroyNotify  value_destroy)
{
   TrieNodeChunk *iter;
   TrieNodeChunk *tmp;

   g_assert(node);

   trie_node_unlink(node);

   while (node->chunk.count) {
      trie_destroy_node(trie, node->chunk.children[0], value_destroy);
   }

   for (iter = node->chunk.next; iter;) {
      tmp = iter;
      iter = iter->next;
      trie_free(trie, tmp);
   }

   if (node->value && value_destroy) {
      value_destroy(node->value);
   }

   trie_free(trie, node);
}

/**
 * trie_new:
 * @value_destroy: A #GDestroyNotify, or %NULL.
 *
 * Creates a new #Trie. When a value is removed from the trie, @value_destroy
 * will be called to allow you to release any resources.
 *
 * Returns: (transfer full): A newly allocated #Trie that should be freed
 *   with trie_free().
 */
Trie *
trie_new (GDestroyNotify value_destroy)
{
   Trie *trie;

#ifdef TRIE_64
   STATIC_ASSERT(sizeof(TrieNode) == 32);
   STATIC_ASSERT(sizeof(TrieNodeChunk) == 16);
#else
   STATIC_ASSERT(sizeof(TrieNode) == 20);
   STATIC_ASSERT(sizeof(TrieNodeChunk) == 12);
#endif

   trie = g_new0(Trie, 1);
   trie->root = trie_node_new(trie, NULL);
   trie->value_destroy = value_destroy;

   return trie;
}

/**
 * trie_insert:
 * @trie: A #Trie.
 * @key: The key to insert.
 * @value: The value to insert.
 *
 * Inserts @value into @trie located with @key.
 */
void
trie_insert (Trie        *trie,
             const gchar *key,
             gpointer     value)
{
   TrieNode *node;

   g_return_if_fail(trie);
   g_return_if_fail(key);
   g_return_if_fail(value);

   node = trie->root;

   while (*key) {
      node = trie_find_or_create_node(trie, node, *key);
      key++;
   }

   if (node->value && trie->value_destroy) {
      trie->value_destroy(node->value);
   }

   node->value = value;
}

/**
 * trie_lookup:
 * @trie: A #Trie.
 * @key: The key to lookup.
 *
 * Looks up @key in @trie and returns the value associated.
 *
 * Returns: (transfer none): The value inserted or %NULL.
 */
gpointer
trie_lookup (Trie        *trie,
             const gchar *key)
{
   TrieNode *node;

   __builtin_prefetch(trie);
   __builtin_prefetch(key);

   g_return_val_if_fail(trie, NULL);
   g_return_val_if_fail(key, NULL);

   node = trie->root;

   while (*key && node) {
      node = trie_find_node(trie, node, *key);
      key++;
   }

   return node ? node->value : NULL;
}

/**
 * trie_remove:
 * @trie: A #Trie.
 * @key: The key to remove.
 *
 * Removes @key from @trie, possibly destroying the value associated with
 * the key.
 *
 * Returns: %TRUE if @key was found, otherwise %FALSE.
 */
gboolean
trie_remove (Trie        *trie,
             const gchar *key)
{
   TrieNode *node;

   g_return_val_if_fail(trie, FALSE);
   g_return_val_if_fail(key, FALSE);

   node = trie->root;

   while (*key && node) {
      node = trie_find_node(trie, node, *key);
      key++;
   }

   if (node && node->value) {
      if (trie->value_destroy) {
         trie->value_destroy(node->value);
      }

      node->value = NULL;

      if (!node->chunk.count) {
         while (node->parent &&
                node->parent->parent &&
                !node->parent->value &&
                (node->parent->chunk.count == 1)) {
            node = node->parent;
         }
         trie_destroy_node(trie, node, trie->value_destroy);
      }

      return TRUE;
   }

   return FALSE;
}

/**
 * trie_traverse_node_pre_order:
 * @trie: A #Trie.
 * @node: A #TrieNode.
 * @str: The prefix for this node.
 * @flags: The flags for which nodes to callback.
 * @max_depth: the maximum depth to process.
 * @func: The func to execute for each matching node.
 * @user_data: User data for @func.
 *
 * Traverses node and all of its children according to the parameters
 * provided. @func is called for each matching node.
 *
 * This assumes that the order is %G_POST_ORDER, and therefore does not
 * have the conditionals to check pre-vs-pre ordering.
 *
 * Returns: %TRUE if traversal was cancelled; otherwise %FALSE.
 */
static gboolean
trie_traverse_node_pre_order (Trie             *trie,
                              TrieNode         *node,
                              GString          *str,
                              GTraverseFlags    flags,
                              gint              max_depth,
                              TrieTraverseFunc  func,
                              gpointer          user_data)
{
   TrieNodeChunk *iter;
   gboolean ret = FALSE;
   guint i;

   g_assert(trie);
   g_assert(node);
   g_assert(str);

   if (max_depth) {
      if ((!node->value && (flags & G_TRAVERSE_NON_LEAVES)) ||
          (node->value && (flags & G_TRAVERSE_LEAVES))) {
         if (func(trie, str->str, node->value, user_data)) {
            return TRUE;
         }
      }
      for (iter = &node->chunk; iter; iter = iter->next) {
         for (i = 0; i < iter->count; i++) {
            g_string_append_c(str, iter->keys[i]);
            if (trie_traverse_node_pre_order(trie,
                                             iter->children[i],
                                             str,
                                             flags,
                                             max_depth - 1,
                                             func,
                                             user_data)) {
               return TRUE;
            }
            g_string_truncate(str, str->len - 1);
         }
      }
   }

   return ret;
}

/**
 * trie_traverse_node_post_order:
 * @trie: A #Trie.
 * @node: A #TrieNode.
 * @str: The prefix for this node.
 * @flags: The flags for which nodes to callback.
 * @max_depth: the maximum depth to process.
 * @func: The func to execute for each matching node.
 * @user_data: User data for @func.
 *
 * Traverses node and all of its children according to the parameters
 * provided. @func is called for each matching node.
 *
 * This assumes that the order is %G_POST_ORDER, and therefore does not
 * have the conditionals to check pre-vs-post ordering.
 *
 * Returns: %TRUE if traversal was cancelled; otherwise %FALSE.
 */
static gboolean
trie_traverse_node_post_order (Trie             *trie,
                               TrieNode         *node,
                               GString          *str,
                               GTraverseFlags    flags,
                               gint              max_depth,
                               TrieTraverseFunc  func,
                               gpointer          user_data)
{
   TrieNodeChunk *iter;
   gboolean ret = FALSE;
   guint i;

   g_assert(trie);
   g_assert(node);
   g_assert(str);

   if (max_depth) {
      for (iter = &node->chunk; iter; iter = iter->next) {
         for (i = 0; i < iter->count; i++) {
            g_string_append_c(str, iter->keys[i]);
            if (trie_traverse_node_post_order(trie,
                                              iter->children[i],
                                              str,
                                              flags,
                                              max_depth - 1,
                                              func,
                                              user_data)) {
               return TRUE;
            }
            g_string_truncate(str, str->len - 1);
         }
      }
      if ((!node->value && (flags & G_TRAVERSE_NON_LEAVES)) ||
          (node->value && (flags & G_TRAVERSE_LEAVES))) {
         ret = func(trie, str->str, node->value, user_data);
      }
   }

   return ret;
}

/**
 * trie_traverse:
 * @trie: A #Trie.
 * @key: The key to start traversal from.
 * @order: The order to traverse.
 * @flags: The flags for which nodes to callback.
 * @max_depth: the maximum depth to process.
 * @func: The func to execute for each matching node.
 * @user_data: User data for @func.
 *
 * Traverses all nodes of @trie according to the parameters. For each node
 * matching the traversal parameters, @func will be executed.
 *
 * Only %G_PRE_ORDER and %G_POST_ORDER are supported for @order.
 *
 * If @max_depth is less than zero, the entire tree will be traversed.
 * If max_depth is 1, then only the root will be traversed.
 */
void
trie_traverse (Trie             *trie,
               const gchar      *key,
               GTraverseType     order,
               GTraverseFlags    flags,
               gint              max_depth,
               TrieTraverseFunc  func,
               gpointer          user_data)
{
   TrieNode *node;
   GString *str;

   g_return_if_fail(trie);
   g_return_if_fail(func);

   node = trie->root;
   key = key ? key : "";

   str = g_string_new(key);

   while (*key && node) {
      node = trie_find_node(trie, node, *key);
      key++;
   }

   if (node) {
      if (order == G_PRE_ORDER) {
         trie_traverse_node_pre_order(trie, node, str, flags,
                                      max_depth, func, user_data);
      } else if (order == G_POST_ORDER) {
         trie_traverse_node_post_order(trie, node, str, flags,
                                       max_depth, func, user_data);
      } else {
         g_warning(_("Traversal order %u is not supported on Trie."), order);
      }
   }

   g_string_free(str, TRUE);
}

/**
 * trie_destroy:
 * @trie: A #Trie or %NULL.
 *
 * Frees @trie and all associated memory.
 */
void
trie_destroy (Trie *trie)
{
   if (trie) {
      trie_destroy_node(trie, trie->root, trie->value_destroy);
      trie->root = NULL;
      trie->value_destroy = NULL;
      g_free(trie);
   }
}
