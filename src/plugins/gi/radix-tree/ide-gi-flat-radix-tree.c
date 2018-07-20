/* ide-gi-flat-radix-tree.c
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

#include "ide-gi-flat-radix-tree.h"

/* Default size of a buffer used to return the matching word in the filter functions */
#define WORD_BUFFER_SIZE 32

G_DEFINE_BOXED_TYPE (IdeGiFlatRadixTree, ide_gi_flat_radix_tree, ide_gi_flat_radix_tree_ref, ide_gi_flat_radix_tree_unref)

/* This tree is designed to be used on mapped memory.
 * Because of performances considerations,
 * only a few validity checks is done on the data.
 */

/**
 * ide_gi_flat_radix_tree_init:
 * @self: an #IdeGiFlatRadixTree
 * @data: a 64 bytes aligned address
 * @len: the data size, mostly for bounds check
 *
 * Initialize a tree from serialized data at @data.
 * As we don't own the data, you can call it safely multiple times.
 *
 * Returns:
 */
void
ide_gi_flat_radix_tree_init (IdeGiFlatRadixTree *self,
                             guint64            *data,
                             gsize               len)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (data != 0);
  g_return_if_fail (len != 0);

  self->data = data;
  self->len = len;
}

/**
 * ide_gi_flat_radix_tree_clear:
 * @self: an #IdeGiFlatRadixTree
 *
 *  Reset the tree as it was new.
 * (so that we can't erroneously read unmapped data)
 *
 * Returns:
 */
void
ide_gi_flat_radix_tree_clear (IdeGiFlatRadixTree *self)
{
  g_return_if_fail (self != NULL);

  self->data = NULL;
  self->len = 0;
}

/* Same as g_str_has_prefix but specifying a prefix size,
 * because we already store the size in the tree.
 */
static inline gboolean
str_has_prefix (const gchar *word,
                const gchar *prefix,
                guint        prefix_len)
{
  g_assert (word != NULL);
  g_assert (prefix != NULL);
  g_assert (prefix_len > 0);

  while (prefix_len > 0)
    {
      gchar ch = *word;

      if (ch != *prefix)
        return FALSE;

      word++;
      prefix++;
      prefix_len--;
    }

  return TRUE;
}

gboolean
ide_gi_flat_radix_tree_lookup (IdeGiFlatRadixTree  *self,
                               const gchar         *word,
                               guint64            **payloads,
                               guint               *nb_payloads)
{
  const gchar *word_ptr = word;
  guint64 *base;
  guint64 *node;
  NodeHeader *header;
  guint32 *children;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (word != NULL && *word != '\0', FALSE);

  if (self->data == NULL)
    {
      g_warning ("The tree is not initialized");
      return FALSE;
    }

  base = node = self->data;

loop:
  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);
  if (header->prefix_size > 0)
    {
      guint64 *payloads_ptr;
      const gchar *prefix;

NO_CAST_ALIGN_PUSH
      payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

      prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      if (!str_has_prefix (word_ptr, prefix, header->prefix_size))
        return FALSE;

      word_ptr += header->prefix_size;
      if (*word_ptr == '\0')
        {
          if (header->nb_payloads == 0)
            return FALSE;

          if (nb_payloads != NULL)
            *nb_payloads = header->nb_payloads;

          if (payloads != NULL)
            *payloads = payloads_ptr;

          return TRUE;
        }
    }

  if (header->nb_children > 0)
    {
      for (guint i = 0; i < header->nb_children; i++, children++)
        {
NO_CAST_SIZE_PUSH
          ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH

          if (child_header->first_char == *word_ptr)
            {
              /* TODO: express offset in 64bits quantity (when serialising)*/
              node = base + (child_header->offset >> 3);
              goto loop;
            }
        }
    }

  return FALSE;
}

static void
iterate_all_from_node (IdeGiFlatRadixTree           *self,
                       guint64                      *node,
                       GString                      *prefix,
                       IdeGiFlatRadixTreeFilterFunc  filter_func,
                       gpointer                      user_data)
{
  NodeHeader *header;
  gsize prefix_len = 0;
  guint32 *children;

  g_assert (self != NULL);
  g_assert (node >= (guint64 *)self->data && node < (guint64 *)self->data + self->len);
  g_assert (filter_func != NULL);

  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);
  if (header->prefix_size > 0)
    {
NO_CAST_ALIGN_PUSH
      guint64 *payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP
      const gchar *node_prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      gsize node_prefix_len = header->prefix_size;

      prefix_len = prefix->len;
      g_string_append_len (prefix, node_prefix, node_prefix_len);

      if (header->nb_payloads > 0)
        filter_func (prefix->str, payloads_ptr, header->nb_payloads, user_data);
    }

  if (header->nb_children > 0)
    {
      for (guint i = 0; i < header->nb_children; i++, children++)
        {
NO_CAST_SIZE_PUSH
          ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH
          guint64 *child;

          child = self->data + (child_header->offset >> 3);
          iterate_all_from_node (self, child, prefix, filter_func, user_data);
        }
    }

  if (header->prefix_size > 0)
    g_string_truncate (prefix, prefix_len);
}

static inline gchar
char_to_lower (gchar ch)
{
  return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}

static inline gboolean
char_case_insensitive_cmp (gchar c1,
                           gchar c2)
{
  c1 = char_to_lower (c1);
  c2 = char_to_lower (c2);

  return c1 == c2;
}

/* If word is NULL or empty, all node match */
static void
find_matching_prefixes (IdeGiFlatRadixTree           *self,
                        const gchar                  *word,
                        IdeGiFlatRadixTreeFilterFunc  filter_func,
                        gpointer                      user_data)
{
  g_autofree gchar *word_buffer = NULL;
  gchar ch_buffer;
  gchar *word_ptr;
  guint64 *base;
  guint64 *node;
  NodeHeader *header;
  guint32 *children;

  g_assert (self != NULL);
  g_assert (filter_func != NULL);

  base = node = self->data;
  if (word == NULL || *word == '\0')
    return;

  word_buffer = g_strdup (word);
  word_ptr = word_buffer;

loop:
  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);

  if (header->prefix_size > 0)
    {
      gsize count;
      guint64 *payloads_ptr;
      const gchar *prefix;

NO_CAST_ALIGN_PUSH
      payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

      prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      count = header->prefix_size;

      while (count > 0)
        {
          if (*word_ptr != *prefix)
            return;

          prefix++;
          word_ptr++;
          count--;

          if (count == 0)
            {
              /* We temporary truncate the word for the filter function */
              ch_buffer = *word_ptr;
              *word_ptr = '\0';
              filter_func (word_buffer, payloads_ptr, header->nb_payloads, user_data);
              *word_ptr = ch_buffer;
            }

          if (*word_ptr == '\0')
            return;
        }
    }

  if (header->nb_children > 0)
    for (guint i = 0; i < header->nb_children; i++, children++)
      {
NO_CAST_SIZE_PUSH
        ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH

        if (child_header->first_char == *word_ptr)
          {
            /* TODO: express offset in 64bits quantity (when serialising)*/
            node = base + (child_header->offset >> 3);
            goto loop;
          }
      }
}

static gchar *
find_matching_branch (IdeGiFlatRadixTree  *self,
                      const gchar         *word,
                      guint64            **node_ptr)
{
  const gchar *word_ptr = word;
  const gchar *node_word_ptr = word;
  guint64 *payloads_ptr;
  const gchar *prefix;
  gsize count;
  guint64 *base;
  guint64 *node;
  NodeHeader *header;
  guint32 *children;

  g_assert (self != NULL);
  g_assert (node_ptr != NULL);

  base = node = self->data;

  if (word == NULL || *word == '\0')
    goto branch_found;

loop:
  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);

  if (header->prefix_size > 0)
    {
NO_CAST_ALIGN_PUSH
      payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

      prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      count = header->prefix_size;

      node_word_ptr = word_ptr;
      while (count > 0)
        {
          if (*word_ptr != *prefix)
            goto failed;

          prefix++;
          word_ptr++;
          count--;

          if (*word_ptr == '\0')
            goto branch_found;
        }
    }

  if (header->nb_children > 0)
    for (guint i = 0; i < header->nb_children; i++, children++)
      {
NO_CAST_SIZE_PUSH
        ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH

        if (child_header->first_char == *word_ptr)
          {
            /* TODO: express offset in 64bits quantity (when serialising)*/
            node = base + (child_header->offset >> 3);
            goto loop;
          }
      }

failed:
  return NULL;

branch_found:
  *node_ptr = node;

  return g_strndup (word, node_word_ptr - word);
}

static void
insensitive_iterate_matching_prefixes (IdeGiFlatRadixTree           *self,
                                       guint64                      *node,
                                       GString                      *word_string,
                                       gsize                         word_pos,
                                       IdeGiFlatRadixTreeFilterFunc  filter_func,
                                       gpointer                      user_data)
{
  g_autoptr(GString) prefix_string = NULL;
  gchar *new_word;
  gchar *word_ptr;
  guint64 *payloads_ptr;
  guint64 *base;
  NodeHeader *header;
  guint32 *children;
  gboolean do_filtering = FALSE;

  g_assert (self != NULL);
  g_assert (node != NULL);
  g_assert (word_string != NULL);

  base = self->data;
  if (word_string->len == 0)
    return;

  /* We copy the word so that we can change it in place to match the case change.
   * This is needed to provide the correct word to the filter function */
  prefix_string = g_string_sized_new (WORD_BUFFER_SIZE);
  g_string_append (prefix_string, word_string->str);

  new_word = prefix_string->str;
  word_ptr = new_word + word_pos;
  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);

  if (header->prefix_size > 0)
    {
      gsize count;
      const gchar *prefix;

NO_CAST_ALIGN_PUSH
      payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

      prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      count = header->prefix_size;

      while (count > 0)
        {
          if (!char_case_insensitive_cmp (*word_ptr, *prefix))
            return;

          /* We change the char in place to match the case */
          *word_ptr = *prefix;
          prefix++;
          word_ptr++;
          count--;

          if (count == 0 && filter_func != NULL)
            do_filtering = TRUE;

          if (*word_ptr == '\0')
            goto postprocess;
        }
    }

  if (header->nb_children > 0)
    for (guint i = 0; i < header->nb_children; i++, children++)
      {
NO_CAST_SIZE_PUSH
        ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH
        guint ch_match_count = 0;

        if (char_case_insensitive_cmp (child_header->first_char, *word_ptr))
          {
            /* TODO: express offset in 64bits quantity (when serialising)*/
            ch_match_count++;
            node = base + (child_header->offset >> 3);
            insensitive_iterate_matching_prefixes (self,
                                                   node,
                                                   prefix_string,
                                                   word_ptr - new_word,
                                                   filter_func,
                                                   user_data);
          }

        /* For a node, there's only two possibilities for a match:
         * lowercase or uppercase, so we exit the loop after that.
         */
        if (ch_match_count > 1)
          break;
      }

postprocess:
  /* we postpone the filter_func call here so that we can avoid a string copy
   * and change it in place to adjust its size.
   */
  if (do_filtering)
    {
      new_word[word_pos + header->prefix_size] = '\0';
      filter_func (new_word, payloads_ptr, header->nb_payloads, user_data);
    }
}

static void
insensitive_iterate_matching_branch (IdeGiFlatRadixTree            *self,
                                     guint64                       *node,
                                     GString                       *word_string,
                                     gsize                          word_pos,
                                     IdeGiFlatRadixTreeFilterFunc   filter_func,
                                     gpointer                       user_data)
{
  g_autoptr(GString) prefix_string = NULL;
  gchar *word_ptr;
  const gchar *node_word_ptr;
  gchar *new_word;
  guint64 *payloads_ptr;
  const gchar *prefix;
  gsize count;
  guint64 *base;
  NodeHeader *header;
  guint32 *children;
  guint ch_match_count = 0;

  g_assert (self != NULL);
  g_assert (node != NULL);
  g_assert (word_string != NULL);

  base = self->data;
  /* We copy the word so that we can change it in place to match the case change */
  prefix_string = g_string_sized_new (WORD_BUFFER_SIZE);

  if (word_string->len == 0)
    {
      node_word_ptr = new_word = prefix_string->str;
      goto branch_found;
    }

  g_string_append (prefix_string, word_string->str);
  new_word = prefix_string->str;
  node_word_ptr = word_ptr = new_word + word_pos;

  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);

  if (header->prefix_size > 0)
    {
NO_CAST_ALIGN_PUSH
      payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

      prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
      count = header->prefix_size;

      node_word_ptr = word_ptr;
      while (count > 0)
        {
          if (!char_case_insensitive_cmp (*word_ptr, *prefix))
            return;

          /* We change the char in place to match the case */
          *word_ptr = *prefix;
          prefix++;
          word_ptr++;
          count--;

          if (*word_ptr == '\0')
            goto branch_found;
        }
    }

  if (header->nb_children > 0)
    for (guint i = 0; i < header->nb_children; i++, children++)
      {
NO_CAST_SIZE_PUSH
        ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH

        if (char_case_insensitive_cmp (child_header->first_char, *word_ptr))
          {
            /* TODO: express offset in 64bits quantity (when serialising)*/
            ch_match_count++;
            node = base + (child_header->offset >> 3);
            insensitive_iterate_matching_branch (self,
                                                 node,
                                                 prefix_string,
                                                 word_ptr - new_word,
                                                 filter_func,
                                                 user_data);
          }

        /* For a node, there's only two possibilities for a match:
         * lowercase or uppercase, so we exit the loop after that.
         */
        if (ch_match_count > 1)
          break;
      }

  return;

branch_found:
  g_string_truncate (prefix_string, node_word_ptr - new_word);
  iterate_all_from_node (self,
                         node,
                         prefix_string,
                         filter_func,
                         user_data);
  return;
}

/**
 * ide_gi_flat_radix_tree_complete_custom:
 *
 * Call the filter_func for every node matching @word.
 * If @word is NULL or empty, filter_func is called
 * on every nodes with a payload.
 *
 * if get_prefix == %FALSE: we get the names equal or longer than @word.
 * if get_prefix == %TRUE: we get the names equal or shorter than @word.
 */
void
ide_gi_flat_radix_tree_complete_custom (IdeGiFlatRadixTree           *self,
                                        const gchar                  *word,
                                        gboolean                      get_prefix,
                                        gboolean                      case_sensitive,
                                        IdeGiFlatRadixTreeFilterFunc  filter_func,
                                        gpointer                      user_data)
{
  g_autoptr(GString) prefix_string = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (filter_func != NULL);

  if (self->data == NULL)
    {
      g_warning ("The tree is not initialized");
      return;
    }

  if (case_sensitive)
    {
      if (!get_prefix)
        {
          g_autofree gchar *common_prefix = NULL;
          guint64 *node = NULL;

          if ((common_prefix = find_matching_branch (self, word, &node)))
            {
              prefix_string = g_string_sized_new (WORD_BUFFER_SIZE);
              g_string_append (prefix_string, common_prefix);
              iterate_all_from_node (self,
                                     node,
                                     prefix_string,
                                     filter_func,
                                     user_data);
            }
        }
      else
        {
          find_matching_prefixes (self, word, filter_func, user_data);
        }
    }
  else
    {
      prefix_string = g_string_sized_new (WORD_BUFFER_SIZE);
      if (word != NULL)
        g_string_append (prefix_string, word);

      if (!get_prefix)
        insensitive_iterate_matching_branch (self, self->data, prefix_string, 0, filter_func, user_data);
      else
        insensitive_iterate_matching_prefixes (self, self->data, prefix_string, 0, filter_func, user_data);
    }
}

static void
complete_item_clear (gpointer data)
{
  IdeGiFlatRadixTreeCompleteItem *item = (IdeGiFlatRadixTreeCompleteItem *)data;

  g_return_if_fail (item != NULL);

  g_free (item->word);
}

static void
complete_get_all_filter_func (const gchar *word,
                              guint64     *payloads,
                              guint        nb_payloads,
                              gpointer     user_data)
{
  GArray *ar = (GArray *)user_data;
  IdeGiFlatRadixTreeCompleteItem item;

  g_assert (word != NULL && *word != '\0');
  g_assert (payloads != NULL);
  g_assert (nb_payloads > 0);

  item.word = g_strdup (word);
  item.payloads = payloads;
  item.nb_payloads = nb_payloads;

  g_array_append_val (ar, item);
}

/* An array of #IdeGiRadixTreeCompleteItem.
 * If @word is NULL or empty, we return all words.
 */
GArray *
ide_gi_flat_radix_tree_complete (IdeGiFlatRadixTree *self,
                                 const gchar        *word,
                                 gboolean            get_prefix,
                                 gboolean            case_sensitive)
{
  GArray *ar;

  g_return_val_if_fail (self != NULL, NULL);

  if (self->data == NULL)
    {
      g_warning ("The tree is not initialized");
      return NULL;
    }

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiFlatRadixTreeCompleteItem));
  g_array_set_clear_func (ar, (GDestroyNotify)complete_item_clear);

  ide_gi_flat_radix_tree_complete_custom (self,
                                          word,
                                          get_prefix,
                                          case_sensitive,
                                          complete_get_all_filter_func,
                                          ar);

  return ar;
}

void
ide_gi_flat_radix_tree_foreach (IdeGiFlatRadixTree           *self,
                                IdeGiFlatRadixTreeFilterFunc  filter_func,
                                gpointer                      user_data)
{
  g_autoptr(GString) prefix_string = NULL;

  g_return_if_fail (self != NULL);
  g_return_if_fail (filter_func != NULL);

  if (self->data == NULL)
    {
      g_warning ("The tree is not initialized");
      return;
    }

  prefix_string = g_string_sized_new (WORD_BUFFER_SIZE);
  iterate_all_from_node (self,
                         self->data,
                         prefix_string,
                         filter_func,
                         user_data);
}

static void
dump (guint64                      *base,
      guint64                      *node,
      const gchar                  *prefix,
      IdeGiFlatRadixTreeFilterFunc  func,
      gpointer                      user_data)
{
  g_autofree gchar *new_prefix = NULL;
  NodeHeader *header;
  guint32 *children;

  g_assert (base != NULL);
  g_assert (node != NULL);

  header = (NodeHeader *)(node);
  children = (guint32 *)(node + 1);

  if (header->prefix_size > 0)
    {
NO_CAST_ALIGN_PUSH
      guint64 *payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP
      g_autofree gchar *node_prefix = g_strndup ((const gchar *)(payloads_ptr + header->nb_payloads), header->prefix_size);

      new_prefix = g_strconcat (prefix, node_prefix, NULL);
      if (header->nb_payloads > 0)
        {
          if (func == NULL)
            {
              g_print ("word:'%s' nb payloads:%d\n", new_prefix, header->nb_payloads);
              for (guint i = 0; i < header->nb_payloads; i++)
                {
                  guint64 *ptr = (guint64 *)payloads_ptr + i;

                  g_print ("%#.8lX\n", *ptr);
                }
            }
          else
            func (new_prefix, payloads_ptr, header->nb_payloads, user_data);
        }
    }
  else
    new_prefix = g_strdup (prefix);

  if (header->nb_children > 0)
    for (guint i = 0; i < header->nb_children; i++, children++)
      {
NO_CAST_SIZE_PUSH
        ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH

        /* TODO: express offset in 64bits quantity (when serialising)*/
        node = base + (child_header->offset >> 3);

        dump (base, node, new_prefix, func, user_data);
      }
}

void
ide_gi_flat_radix_tree_dump (IdeGiFlatRadixTree           *self,
                             IdeGiFlatRadixTreeFilterFunc  func,
                             gpointer                      user_data)
{
  g_autofree gchar *prefix = NULL;
  guint64 *base = self->data;

  g_return_if_fail (self != NULL);

  if (base == NULL)
    {
      g_warning ("The tree is not initialized");
      return;
    }

  prefix = g_strdup ("\0");
  dump (base, base, prefix, func, user_data);
}

static IdeGiRadixTreeNode *
deserialize_node (IdeGiRadixTreeNode *dst_parent_node,
                  guint64            *src_node,
                  guint64            *base)
{
  IdeGiRadixTreeNode *child_node;
  NodeHeader *header = (NodeHeader *)src_node;
  guint32 *children;
  guint64 *payloads_ptr;
  const gchar *prefix;

  g_assert (src_node != NULL);
  g_assert (base != NULL);

  children = (guint32 *)(src_node + 1);

NO_CAST_ALIGN_PUSH
  payloads_ptr = (guint64 *)(children + header->nb_children + (header->nb_children & 1));
NO_CAST_ALIGN_POP

  prefix = (const gchar *)(payloads_ptr + header->nb_payloads);
  child_node = _ide_gi_radix_tree_builder_node_add (dst_parent_node,
                                                    prefix,
                                                    header->prefix_size,
                                                    header->nb_payloads,
                                                    payloads_ptr);

  if (header->nb_children > 0)
    {
      for (guint i = 0; i < header->nb_children; i++, children++)
        {
NO_CAST_SIZE_PUSH
          ChildHeader *child_header = (ChildHeader *)children;
NO_CAST_SIZE_PUSH
          guint64 *child;

          child = base + (child_header->offset >> 3);
          deserialize_node (child_node, child, base);
        }
    }

  return child_node;
}

IdeGiRadixTreeBuilder *
ide_gi_flat_radix_tree_deserialize (IdeGiFlatRadixTree *self)
{
  IdeGiRadixTreeBuilder *tree;
  IdeGiRadixTreeNode *root;

  g_return_val_if_fail (self != NULL, NULL);

  if (self->data == NULL)
    {
      g_warning ("The tree is not initialized");
      return NULL;
    }

  tree = ide_gi_radix_tree_builder_new ();
  root = deserialize_node (NULL, self->data, self->data);
  _ide_gi_radix_tree_builder_set_root (tree, root);

  return tree;
}

IdeGiFlatRadixTree *
ide_gi_flat_radix_tree_new (void)
{
  IdeGiFlatRadixTree *self;

  self = g_slice_new0 (IdeGiFlatRadixTree);
  self->ref_count = 1;

  return self;
}

static void
ide_gi_flat_radix_tree_free (IdeGiFlatRadixTree *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_slice_free (IdeGiFlatRadixTree, self);
}

IdeGiFlatRadixTree *
ide_gi_flat_radix_tree_ref (IdeGiFlatRadixTree *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_flat_radix_tree_unref (IdeGiFlatRadixTree *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_flat_radix_tree_free (self);
}
