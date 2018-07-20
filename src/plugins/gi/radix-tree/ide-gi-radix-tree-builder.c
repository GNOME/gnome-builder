/* ide-gi-radix-tree-builder.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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
#include <stdio.h>
#include <string.h>

#include "ide-gi-radix-tree-common.h"

#include "ide-gi-radix-tree-builder.h"

#include <dazzle.h>

/* Limitations of the serialized format:
 * - not endian agnostic
 * - prefix size < 256 bytes
 * - nb childs < 256
 * - nb 64b payloads per node < 256
 *
 * The variable size payload permit us to inline data
 * next to the node in the serialized format.
*/
struct _IdeGiRadixTreeBuilder
{
  GObject             parent_instance;

  IdeGiRadixTreeNode *root;
  const gchar        *orig_word;
};

typedef struct _SerializeState
{
  GByteArray *ar;
} SerializeState;

G_STATIC_ASSERT (sizeof (NodeHeader) == sizeof (guint64));

G_DEFINE_TYPE (IdeGiRadixTreeBuilder, ide_gi_radix_tree_builder, G_TYPE_OBJECT)

G_GNUC_UNUSED static inline guint32
serialize_add_uint32 (SerializeState *state,
                      guint32         value)
{
  guint32 offset;

  g_assert (state != NULL);

  offset = state->ar->len;
  g_byte_array_append (state->ar, (guint8 *)&value, sizeof (guint32));

  return offset;
}

static inline void
serialize_set_uint32 (SerializeState *state,
                      guint32         offset,
                      guint32         value)
{
  guint32 *ptr;

  g_assert (state != NULL);

NO_CAST_ALIGN_PUSH
  ptr = (guint32 *)(state->ar->data + offset);
  *ptr = value;
NO_CAST_ALIGN_POP
}

static inline guint32
serialize_add_uint64 (SerializeState *state,
                      guint64         value)
{
  guint32 offset;

  g_assert (state != NULL);

  offset = state->ar->len;
  g_byte_array_append (state->ar, (guint8 *)&value, sizeof (guint64));

  return offset;
}

static inline guint32
serialize_grow_n_uint32 (SerializeState *state,
                         guint32         nb)
{
  guint32 offset;
  guint8 *pos;
  gsize len;
  gsize new_len;

  g_assert (state != NULL);
  g_assert (nb > 0);

  offset = state->ar->len;
  len = nb * sizeof (guint32);
  new_len = state->ar->len + len;

  g_byte_array_set_size (state->ar, new_len);
  pos = state->ar->data + offset;
  memset (pos, 0, len);

  return offset;
}

static inline guint32
serialize_add_string (SerializeState *state,
                      gchar          *str,
                      guint32        *new_end)
{
  guint32 offset;
  gsize len;
  guint32 padding;
  guint32 new_len;
  guint32 pattern = 0;

  g_assert (state != NULL);
  g_assert (str != NULL && *str != '\0');

  offset = state->ar->len;
  len = strlen (str);
  padding = (8 - (len & 7)) & 7;
  new_len = state->ar->len + len + padding;

  g_byte_array_append (state->ar, (guint8 *)str, len);
  g_byte_array_append (state->ar, (guint8 *)&pattern, padding);

  if (new_end != NULL)
    *new_end = new_len;

  return offset;
}

static guint32
serialize_node (SerializeState     *state,
                IdeGiRadixTreeNode *node)
{
  NodeHeader header = {0};
  guint32 offset;
  guint nb_children;
  guint32 children_ref_base = 0;
  guint64 val;

  g_assert (state != NULL);
  g_assert (node != NULL);

  nb_children = (node->children != NULL) ? node->children->len : 0;
  header.nb_children = nb_children;

  if (node->prefix != NULL)
    header.prefix_size = strlen (node->prefix);

  if (node->nb_payloads > 0)
    {
      g_assert (node->payloads != NULL);
      header.nb_payloads = node->nb_payloads;
    }

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
  val = header.prefix_size | (header.nb_children << 8) | (header.nb_payloads << 16);
#else
  val = header.prefix_size << 56 | (header.nb_children << 48) | (header.nb_payloads << 40);
#endif

  offset = serialize_add_uint64 (state, val);

  /* Reserve space for the children refs + padding */
  if (nb_children > 0)
    children_ref_base = serialize_grow_n_uint32 (state, nb_children + (nb_children & 1));

  if (node->nb_payloads > 0)
    for (guint i = 0; i < node->nb_payloads; i++)
      serialize_add_uint64 (state, node->payloads[i]);

  /* We pad the string to get a 64b alignment.
     Because we have a prefix_size field, the string is not zero terminated.
   */
  if (node->prefix != NULL)
    serialize_add_string (state, node->prefix, NULL);

  if (nb_children > 0)
    {
      for (guint i = 0; i < nb_children; i++)
        {
          IdeGiRadixTreeNode *child;
          ChildHeader child_header;
          guint32 value;

          child = g_ptr_array_index (node->children, i);
          child_header.offset = serialize_node (state, child);
          child_header.first_char = *child->prefix;

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
          value = child_header.first_char | (child_header.offset << 8);
#else
          value = child_header.offset | (child_header.first_char << 24);
#endif

          serialize_set_uint32 (state, children_ref_base + i * 4, value);
        }
    }

  return offset;
}

/**
 * ide_gi_radix_tree_builder_serialize:
 *
 * Returns: (transfer full) #GByteArray
 */
GByteArray *
ide_gi_radix_tree_builder_serialize (IdeGiRadixTreeBuilder *self)
{
  SerializeState *state;
  GByteArray *ar;

  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), NULL);

  if (self->root == NULL)
    {
      g_warning ("Can't serialize an empty radix tree");
      return NULL;
    }

  state = g_slice_new0 (SerializeState);
  state->ar = g_byte_array_new ();

  serialize_node (state, self->root);
  /* We add a 0 uint64 at the end to be sure we don't acces past allocated memory,
   * see str_has_prefix in ide-gi-flat-radix-tree.c implementation.
   */
  serialize_add_uint64 (state, 0);

  ar = state->ar;
  g_slice_free (SerializeState, state);

  return ar;
}

static void
node_free (IdeGiRadixTreeNode *node)
{
  g_assert (node != NULL);

  g_free  (node->prefix);
  dzl_clear_pointer (&node->children, g_ptr_array_unref);
  dzl_clear_pointer (&node->payloads, g_free);
  g_slice_free (IdeGiRadixTreeNode, node);
}

/* Return < 0 if c1 < c2, == 0 if c1 == c2 and > 0 if c1 > c2,
 * placing a upper case char just before the lower case one.
 */
static inline gint
ascii_cmp (gchar c1,
           gchar c2)
{
  gchar c1_lower = c1;
  gchar c2_lower = c2;

  if (c1 == c2)
    return 0;

  if (c1 >= 0x41 && c1 <= 0x5A)
    c1_lower += 0x20;

  if (c2 >= 0x41 && c2 <= 0x5A)
    c2_lower += 0x20;

  if (c1_lower == c2_lower)
    return -1;
  else
    return (c1_lower - c2_lower);
}

/**
 * ide_gi_radix_tree_builder_node_add:
 *
 * If prefix_size == -1, the string is NULL terminated,
 * otherwise prefix_size bytes are used for the prefix.
 *
 * Returns: a new #IdeGiRadixTreeNode
 */
IdeGiRadixTreeNode *
_ide_gi_radix_tree_builder_node_add (IdeGiRadixTreeNode *parent,
                                     const gchar        *prefix,
                                     gint                prefix_size,
                                     guint               nb_payloads,
                                     gpointer            payloads)
{
  IdeGiRadixTreeNode *child;
  gsize len;

  child = g_slice_new0 (IdeGiRadixTreeNode);
  child->parent = parent;

  if (nb_payloads > 0)
    {
      g_assert (payloads != NULL);

      child->nb_payloads = nb_payloads;
      len = nb_payloads * sizeof (guint64);
      child->payloads = g_malloc (len);
      memcpy (child->payloads, payloads, len);
    }

  if (prefix_size < 0)
    {
      g_assert (strlen (prefix) <= 255);
      child->prefix = g_strdup (prefix);
    }
  else if (prefix_size > 0)
    {
      g_assert (prefix_size <= 255);
      child->prefix = g_strndup (prefix, prefix_size);
    }

  if (parent != NULL)
    {
      g_assert (prefix != NULL && *prefix != '\0');

      if (parent->children == NULL)
        {
          parent->children = g_ptr_array_new_with_free_func ((GDestroyNotify)node_free);
          g_ptr_array_add (parent->children, child);
        }
      else
        {
          /* Add the child in ascii alphabetical order */
          for (guint i = 0; i < parent->children->len; i++)
            {
              IdeGiRadixTreeNode *tmp_child;

              tmp_child = g_ptr_array_index (parent->children, i);
              if (ascii_cmp (*prefix, *(tmp_child->prefix)) <= 0)
                {
                  g_ptr_array_insert (parent->children, i, child);
                  return child;
                }
            }

          g_ptr_array_add (parent->children, child);
        }
    }

  return child;
}

/* word_pos indicate the position where the split
 * need to happen in both word and node->prefix.
 */
static void
node_split (IdeGiRadixTreeNode *node,
            const gchar        *word,
            gsize               word_pos,
            guint               nb_payloads,
            gpointer            payloads)
{
  IdeGiRadixTreeNode *child_right;
  GPtrArray *children;
  gchar *prefix;
  const gchar *suffix_left;
  const gchar *suffix_right;
  gsize len;

  g_assert (node != NULL);
  g_assert (word != NULL && *word != '\0');

  prefix = (word_pos != 0) ? g_strndup (node->prefix, word_pos) : NULL;

  children = node->children;
  node->children = NULL;

  suffix_left  = word + word_pos;
  suffix_right  = node->prefix + word_pos;

  /* If the word is not fully consumed */
  if (*suffix_left != '\0')
    _ide_gi_radix_tree_builder_node_add (node, suffix_left, -1, nb_payloads, payloads);

  child_right = _ide_gi_radix_tree_builder_node_add (node, suffix_right, -1, node->nb_payloads, node->payloads);
  node->nb_payloads = 0;
  dzl_clear_pointer (&node->payloads, g_free);
  child_right->children = children;

  /* We need to re-parent the children */
  if (children != NULL)
    for (guint i = 0; i < children->len; i++)
      {
        IdeGiRadixTreeNode *child = g_ptr_array_index (children, i);

        child->parent = child_right;
      }

  g_free (node->prefix);
  node->prefix = prefix;

  /* If the word is fully consumed */
  if (*suffix_left == '\0')
    {
      g_assert (nb_payloads > 0 && payloads != NULL);

      node->nb_payloads = nb_payloads;
      len = nb_payloads * sizeof (guint64);
      node->payloads = g_malloc (len);
      memcpy (node->payloads, payloads, len);
    }
}

gboolean
ide_gi_radix_tree_builder_node_insert_payload (IdeGiRadixTreeNode *node,
                                               guint               pos,
                                               guint               nb_payloads,
                                               gpointer            payloads)
{
  guint new_size;
  guint64 *new_payload;
  guint64 *new_src = payloads;
  guint64 *old_src = node->payloads;
  guint64 *new_dst;
  guint count = 0;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail ((node->nb_payloads > 0 && pos <= node->nb_payloads + 1) ||
                        (node->nb_payloads == 0 && pos == 0),
                        FALSE);
  g_return_val_if_fail (nb_payloads > 0, FALSE);

  if (node->nb_payloads == 0)
    {
      node->nb_payloads = nb_payloads;
      node->payloads = g_memdup (payloads, nb_payloads * sizeof (guint64));
      return TRUE;
    }

  if (pos > node->nb_payloads)
    {
      g_warning ("Can't insert payload at pos %d, that is beyond the payload size", pos);
      return FALSE;
    }

  new_size = node->nb_payloads + nb_payloads;
  new_dst = new_payload = g_malloc (new_size * sizeof (guint64));

  while (count < pos)
    {
      *new_dst++ = *old_src++;
      count++;
    }

  for (guint nb = 0; nb < nb_payloads; nb++)
    *new_dst++ = *new_src++;

  while (count < node->nb_payloads)
    {
      *new_dst++ = *old_src++;
      count++;
    }

  g_free (node->payloads);
  node->nb_payloads = new_size;
  node->payloads = new_payload;

  return TRUE;
}

/* We return FALSE if the word is already inserted in the tree */
static gboolean
add_word_from_node (IdeGiRadixTreeBuilder *self,
                    IdeGiRadixTreeNode    *node,
                    const gchar           *word,
                    guint                  nb_payloads,
                    gpointer               payloads)
{
  const gchar *word_ptr = word;
  const gchar *prefix_ptr;
  gsize pos = 0;

  g_assert (IDE_IS_GI_RADIX_TREE_BUILDER (self));
  g_assert (node != NULL);
  g_assert (word != NULL && *word !=  '\0');

  prefix_ptr = node->prefix;
  while (*word_ptr  != '\0')
    {
      if (prefix_ptr == NULL || *prefix_ptr == '\0')
        {
          if (node->children != NULL)
            {
              for (guint i = 0; i < node->children->len; i++)
                {
                  IdeGiRadixTreeNode *child = g_ptr_array_index (node->children, i);

                  if (*word_ptr == *(child->prefix))
                    return add_word_from_node (self, child, word_ptr, nb_payloads, payloads);
                }
            }

          _ide_gi_radix_tree_builder_node_add (node, word_ptr, -1, nb_payloads, payloads);
          return TRUE;
        }
      else if (*word_ptr != *prefix_ptr)
        {
          node_split (node, word, pos, nb_payloads, payloads);
          return TRUE;
        }

      word_ptr++;
      prefix_ptr++;
      pos++;
    }

  if (*prefix_ptr == '\0')
    {
      if (node->nb_payloads > 0)
        {
          /* TODO: add an allow_append arg so that we can append payload if the node exist */
          g_warning ("IdeGiRadixTreeBuilder: can't add duplicate:'%s'", self->orig_word);
          return FALSE;
        }
      else
        ide_gi_radix_tree_builder_node_insert_payload (node, 0, nb_payloads, payloads);
    }
  else
    node_split (node, word, pos, nb_payloads, payloads);

  return TRUE;
}

gboolean
ide_gi_radix_tree_builder_add (IdeGiRadixTreeBuilder *self,
                               const gchar           *word,
                               guint                  nb_payloads,
                               gpointer               payloads)
{
  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), FALSE);
  g_return_val_if_fail (word != NULL && *word != '\0', FALSE);
  g_return_val_if_fail (nb_payloads > 0, FALSE);
  g_return_val_if_fail (payloads != NULL, FALSE);

  if (strlen (word) > 255)
    {
      g_warning ("The implementation require a word size <= 255 bytes");
      return FALSE;
    }

  self->orig_word = word;

  if (self->root == NULL)
    {
      self->root = _ide_gi_radix_tree_builder_node_add (NULL, word, -1, nb_payloads, payloads);
      return TRUE;
    }

  return add_word_from_node (self, self->root, word, nb_payloads, payloads);
}

/* index is a location to return the index in the parent.
 * If the result of this function is NULL or if the result's node
 * has no parent (root), the index content is undefined */
static IdeGiRadixTreeNode *
lookup_word_from_node (IdeGiRadixTreeBuilder *self,
                       IdeGiRadixTreeNode    *node,
                       const gchar           *word,
                       guint                 *index)
{
  const gchar *word_ptr = word;

  g_assert (IDE_IS_GI_RADIX_TREE_BUILDER (self));
  g_assert (node != NULL);
  g_assert (word != NULL && *word !=  '\0');

  if (node->prefix != NULL)
    {
      if (!g_str_has_prefix (word_ptr, node->prefix))
        goto missing;

      word_ptr += strlen (node->prefix);
      if (*word_ptr == '\0')
        {
          if (node->nb_payloads > 0)
            return node;

          goto missing;
        }
    }

  if (node->children != NULL)
    {
      for (guint i = 0; i < node->children->len; i++)
        {
          IdeGiRadixTreeNode *child;

          child = g_ptr_array_index (node->children, i);
          if (*word_ptr == *(child->prefix))
            {
              if (index != NULL)
                *index = i;

              return lookup_word_from_node (self, child, word_ptr, index);
            }
        }
    }

missing:
  return NULL;
}

IdeGiRadixTreeNode *
ide_gi_radix_tree_builder_lookup (IdeGiRadixTreeBuilder *self,
                                  const gchar           *word)
{
  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), NULL);
  g_return_val_if_fail (word != NULL && *word != '\0', NULL);

  if (self->root == NULL)
    return NULL;

  return lookup_word_from_node (self, self->root, word, NULL);
}

static void
get_all_from_node (IdeGiRadixTreeBuilder *self,
                   IdeGiRadixTreeNode    *node,
                   GArray                *ar,
                   const gchar           *prefix)
{
  IdeGiRadixTreeCompleteItem item;
  gchar *full_word;
  gsize len;

  g_assert (IDE_IS_GI_RADIX_TREE_BUILDER (self));
  g_assert (node != NULL);
  g_assert (ar != NULL);
  g_assert (prefix != NULL);

  if (node->prefix != NULL)
    {
      full_word = g_strconcat (prefix, node->prefix, NULL);

      if (node->nb_payloads > 0)
        {
          item.word = full_word;

          len = node->nb_payloads * sizeof (guint64);
          item.nb_payloads = node->nb_payloads;
          item.payloads = g_malloc (len);
          memcpy (item.payloads, node->payloads, len);

          g_array_append_val (ar, item);
        }
    }
  else
    {
      full_word = (gchar *)prefix;
    }

  if (node->children != NULL)
    {
      for (guint i = 0; i < node->children->len; i++)
        {
          IdeGiRadixTreeNode *child;

          child = g_ptr_array_index (node->children, i);
          get_all_from_node (self, child, ar, full_word);
        }
    }

  if (node->nb_payloads == 0)
    g_free (full_word);
}

static inline IdeGiRadixTreeNode *
pick_next_child (IdeGiRadixTreeNode *node,
                 gchar               ch)
{
  IdeGiRadixTreeNode *found = NULL;

  g_assert (node != NULL);

  if (node->children != NULL)
    {
      for (guint i = 0; i < node->children->len; i++)
        {
          IdeGiRadixTreeNode *child;

          child = g_ptr_array_index (node->children, i);
          if (*child->prefix == ch)
            {
              found = child;
              break;
            }
        }
    }

  return found;
}

static void
complete_item_free_content (gpointer data)
{
  IdeGiRadixTreeCompleteItem *item = (IdeGiRadixTreeCompleteItem *)data;

  g_assert (item != NULL);

  g_free (item->word);
  g_free (item->payloads);
}

/* An array of #IdeGiRadixTreeCompleteItem.
 * If word is NULL or empty we return all words.
 */
GArray *
ide_gi_radix_tree_builder_complete (IdeGiRadixTreeBuilder *self,
                                    const gchar           *word)
{
  g_autofree gchar *new_word = NULL;
  const gchar *word_ptr = word;
  const gchar *node_word_ptr = word;
  GArray *ar;
  IdeGiRadixTreeNode *node = self->root;

  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), NULL);

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiRadixTreeCompleteItem));
  g_array_set_clear_func (ar, (GDestroyNotify)complete_item_free_content);

  if (self->root == NULL)
    goto failed;

  if (word == NULL || *word == '\0')
    goto get_all;

  while (node != NULL)
    {
      if (node->prefix != NULL)
        {
          const gchar *prefix_ptr = node->prefix;

          node_word_ptr = word_ptr;
          while (*prefix_ptr != '\0')
            {
              if (*word_ptr != *prefix_ptr)
                goto failed;

              prefix_ptr++;
              word_ptr++;

              if (*word_ptr == '\0')
                goto get_all;
            }
        }

      node = pick_next_child (node, *word_ptr);
    }

failed:
  return ar;

get_all:
  new_word = g_strndup (word, node_word_ptr - word);
  get_all_from_node (self, node, ar, new_word);
  return ar;
}

gboolean
ide_gi_radix_tree_builder_remove (IdeGiRadixTreeBuilder *self,
                                  const gchar           *word)
{
  IdeGiRadixTreeNode *node;
  guint index = 0;

  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), FALSE);
  g_return_val_if_fail (word != NULL && *word !=  '\0', FALSE);

  if (self->root == NULL)
    return FALSE;

  if (NULL == (node = lookup_word_from_node (self, self->root, word, &index)))
    return FALSE;

  if (node->children == NULL)
    {
      if (node == self->root)
        dzl_clear_pointer (&self->root, node_free);
      else
        g_ptr_array_remove_index_fast (node->parent->children, index);
    }
  else
    {
      /* Try to compress this node with an unique child */
      if (node->children->len == 1)
        {
          IdeGiRadixTreeNode *child;
          gchar *new_prefix;

          child = g_ptr_array_index (node->children, 0);
          new_prefix = g_strconcat (node->prefix, child->prefix, NULL);
          g_free (child->prefix);

          child->prefix = new_prefix;
          child->parent = node->parent;

          g_ptr_array_set_free_func (node->children, NULL);
          g_ptr_array_remove_index_fast (node->parent->children, index);

          g_ptr_array_add (child->parent->children, child);
        }
      else
        {
          node->nb_payloads = 0;
          dzl_clear_pointer (&node->payloads, g_free);
        }
    }

  return TRUE;
}

gboolean
ide_gi_radix_tree_builder_node_append_payload (IdeGiRadixTreeNode *node,
                                               guint               nb_payloads,
                                               gpointer            payloads)
{
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (nb_payloads > 0, FALSE);
  g_return_val_if_fail (payloads != NULL, FALSE);

  /* TODO: try a g_realloc version */
  return ide_gi_radix_tree_builder_node_insert_payload (node, node->nb_payloads, nb_payloads, payloads);
}

gboolean
ide_gi_radix_tree_builder_node_prepend_payload   (IdeGiRadixTreeNode *node,
                                                  guint               nb_payloads,
                                                  gpointer            payloads)
{
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (nb_payloads > 0, FALSE);
  g_return_val_if_fail (payloads != NULL, FALSE);

  /* TODO: try a g_realloc version */
  return ide_gi_radix_tree_builder_node_insert_payload (node, 0, nb_payloads, payloads);
}

gboolean
ide_gi_radix_tree_builder_node_remove_payload (IdeGiRadixTreeNode *node,
                                               guint               pos)
{
  guint new_size;
  guint64 *new_payload;
  guint64 *old_src = node->payloads;
  guint64 *new_dst;
  guint count = 0;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (node->nb_payloads > 0, FALSE);

  if (pos > node->nb_payloads - 1)
    {
      g_warning ("Can't remove payload at pos %d, that is beyond the payload size", pos);
      return FALSE;
    }

  new_size = node->nb_payloads - 1;
  new_dst = new_payload = g_malloc (new_size * sizeof (guint64));

  while (count < pos)
    {
      *new_dst++ = *old_src++;
      count++;
    }

  old_src++;

  while (count < new_size)
    {
      *new_dst++ = *old_src++;
      count++;
    }

  g_free (node->payloads);
  node->nb_payloads = new_size;
  node->payloads = new_payload;

  return TRUE;
}

/**
 * ide_gi_radix_tree_builder_set_root:
 *
 * The root can only be set on a very new tree,
 * this method is used by the flat tree deserializer.
 *
 * Returns:
 */
gboolean
_ide_gi_radix_tree_builder_set_root (IdeGiRadixTreeBuilder *self,
                                     IdeGiRadixTreeNode    *node)
{
  g_return_val_if_fail (self->root == NULL, FALSE);

  self->root = node;

  return TRUE;
}

static void
dump (IdeGiRadixTreeNode *node,
      const gchar        *word)
{
  g_autofree gchar *full_word = NULL;

  g_assert (node != NULL);

  if (word != NULL)
    {
      if (node->prefix != NULL)
        full_word = g_strconcat (word, node->prefix, NULL);
      else
        full_word = g_strdup (word);
    }
  else
    {
      if (node->prefix != NULL)
        full_word = g_strdup (node->prefix);
      else
        full_word = g_strdup ("\0");
    }

  if (node->nb_payloads > 0)
    {
      g_print ("Word:'%s' nb payloads:%d\n", full_word, node->nb_payloads);
      for (guint i = 0; i < node->nb_payloads; i++)
        {
          guint64 *ptr = (guint64 *)node->payloads + i;

          g_print ("%#.8lX\n", *ptr);
        }
    }

  if (node->children != NULL)
    for (guint i = 0; i < node->children->len; i++)
      dump (g_ptr_array_index (node->children, i), full_word);
}

void
ide_gi_radix_tree_builder_dump (IdeGiRadixTreeBuilder *self)
{
  IdeGiRadixTreeNode *node = self->root;

  g_return_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self));

  if (node == NULL)
    g_print ("the radix tree is empty\n");
  else
    dump (node, NULL);
}

static void
dump_node (IdeGiRadixTreeNode *node,
           gint                indent)
{
  g_assert (node != NULL);

  printf ("%*s|_ node %p:'%s'", indent, " ", node, node->prefix);
  if (node->nb_payloads > 0)
    {
      g_print ("nb payloads:%d\n", node->nb_payloads);
      for (guint i = 0; i < node->nb_payloads; i++)
        {
          guint64 *ptr = (guint64 *)node->payloads + i;

          g_print ("%*s   %#.8lX\n", indent, " ", *ptr);
        }
    }

  if (node->children != NULL)
    {
      g_print (" childs (%d):\n", node->children->len);
      for (gint i = 0; i < node->children->len; i++)
        dump_node (g_ptr_array_index (node->children, i), indent + 2);
    }
  else
    printf ("\n");
}

void
ide_gi_radix_tree_builder_dump_nodes (IdeGiRadixTreeBuilder *self)
{
  IdeGiRadixTreeNode *node = self->root;

  g_return_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self));

  if (node == NULL)
    g_print ("The radix tree is empty\n");
  else
    dump_node (node, 1);
}

gboolean
ide_gi_radix_tree_builder_is_empty (IdeGiRadixTreeBuilder  *self)
{
  g_return_val_if_fail (IDE_IS_GI_RADIX_TREE_BUILDER (self), FALSE);

  return (self->root == NULL);
}

IdeGiRadixTreeBuilder *
ide_gi_radix_tree_builder_new (void)
{
  return g_object_new (IDE_TYPE_GI_RADIX_TREE_BUILDER, NULL);
}

static void
ide_gi_radix_tree_builder_finalize (GObject *object)
{
  IdeGiRadixTreeBuilder *self = (IdeGiRadixTreeBuilder *)object;

  dzl_clear_pointer (&self->root, node_free);

  G_OBJECT_CLASS (ide_gi_radix_tree_builder_parent_class)->finalize (object);
}

static void
ide_gi_radix_tree_builder_class_init (IdeGiRadixTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_radix_tree_builder_finalize;
}

static void
ide_gi_radix_tree_builder_init (IdeGiRadixTreeBuilder *self)
{
}
