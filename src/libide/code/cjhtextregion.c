/* cjhtextregion.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "cjhtextregionprivate.h"
#include "cjhtextregionbtree.h"

/**
 * SECTION:cjhtextregion
 * @Title: CjhTextRegion
 * @Short_description: track regions of text with a hybrid B+Tree and piecetable
 *
 * This data-structure is a hybrid between a PieceTable and a B+Tree, which I've
 * decided to call a Piece+Tree. It allows for very fast tracking of regions of
 * text (in a single dimension, meaning no sub-regions).
 *
 * This is very useful for tracking where work still needs to be done in a text
 * buffer such as for spelling mistakes, syntax highlighting, error checking, or
 * multi-device synchronization.
 *
 * See_also: https://blogs.gnome.org/chergert/2021/03/26/bplustree_augmented_piecetable/
 */

#ifndef G_DISABLE_ASSERT
# define DEBUG_VALIDATE(a,b) G_STMT_START { if (a) cjh_text_region_node_validate(a,b); } G_STMT_END
#else
# define DEBUG_VALIDATE(a,b) G_STMT_START { } G_STMT_END
#endif

static inline void
cjh_text_region_invalid_cache (CjhTextRegion *region)
{
  region->cached_result = NULL;
  region->cached_result_offset = 0;
}

G_GNUC_UNUSED static void
cjh_text_region_node_validate (CjhTextRegionNode *node,
                               CjhTextRegionNode *parent)
{
  gsize length = 0;
  gsize length_in_parent = 0;

  g_assert (node != NULL);
  g_assert (UNTAG (node->tagged_parent) == parent);
  g_assert (cjh_text_region_node_is_leaf (node) ||
            UNTAG (node->tagged_parent) == node->tagged_parent);
  g_assert (!parent || !cjh_text_region_node_is_leaf (parent));
  g_assert (!parent || !SORTED_ARRAY_IS_EMPTY (&parent->branch.children));

  if (parent != NULL)
    {
      SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
        if (child->node == node)
          {
            length_in_parent = child->length;
            goto found;
          }
      });
      g_assert_not_reached ();
    }
found:

  if (parent != NULL)
    g_assert_cmpint (length_in_parent, ==, cjh_text_region_node_length (node));

  for (CjhTextRegionNode *iter = parent;
       iter != NULL;
       iter = cjh_text_region_node_get_parent (iter))
    g_assert_false (cjh_text_region_node_is_leaf (iter));

  if (cjh_text_region_node_is_leaf (node))
    {
      SORTED_ARRAY_FOREACH (&node->leaf.runs, CjhTextRegionRun, run, {
        g_assert_cmpint (run->length, >, 0);
        length += run->length;
      });

      if (node->leaf.prev != NULL)
        g_assert_true (cjh_text_region_node_is_leaf (node->leaf.prev));

      if (node->leaf.next != NULL)
        g_assert_true (cjh_text_region_node_is_leaf (node->leaf.next));
    }
  else
    {
      SORTED_ARRAY_FOREACH (&node->branch.children, CjhTextRegionChild, child, {
        CjhTextRegionChild *next = SORTED_ARRAY_FOREACH_PEEK (&node->branch.children);

        g_assert_nonnull (child->node);
        g_assert_cmpint (child->length, >, 0);
        g_assert_cmpint (child->length, ==, cjh_text_region_node_length (child->node));
        g_assert_true (cjh_text_region_node_get_parent (child->node) == node);

        length += child->length;

        if (next != NULL && next->node)
          {
            g_assert_cmpint (cjh_text_region_node_is_leaf (child->node), ==,
                             cjh_text_region_node_is_leaf (next->node));

            if (cjh_text_region_node_is_leaf (child->node))
              {
                g_assert_true (child->node->leaf.next == next->node);
                g_assert_true (child->node == next->node->leaf.prev);
              }
            else
              {
                g_assert_true (child->node->branch.next == next->node);
                g_assert_true (child->node == next->node->branch.prev);
              }
          }
      });
    }

  if (parent != NULL)
    g_assert_cmpint (length_in_parent, ==, length);
}

static void
cjh_text_region_split (CjhTextRegion          *region,
                       gsize                   offset,
                       const CjhTextRegionRun *run,
                       CjhTextRegionRun       *left,
                       CjhTextRegionRun       *right)
{
  if (region->split_func != NULL)
    region->split_func (offset, run, left, right);
}

static CjhTextRegionNode *
cjh_text_region_node_new (CjhTextRegionNode *parent,
                          gboolean           is_leaf)
{
  CjhTextRegionNode *node;

  g_assert (UNTAG (parent) == parent);

  node = g_new0 (CjhTextRegionNode, 1);
  node->tagged_parent = TAG (parent, is_leaf);

  if (is_leaf)
    {
      SORTED_ARRAY_INIT (&node->leaf.runs);
      node->leaf.prev = NULL;
      node->leaf.next = NULL;
    }
  else
    {
      SORTED_ARRAY_INIT (&node->branch.children);
    }

  g_assert (cjh_text_region_node_get_parent (node) == parent);

  return node;
}

static void
cjh_text_region_subtract_from_parents (CjhTextRegion     *region,
                                       CjhTextRegionNode *node,
                                       gsize              length)
{
  CjhTextRegionNode *parent = cjh_text_region_node_get_parent (node);

  if (parent == NULL || length == 0)
    return;

  cjh_text_region_invalid_cache (region);

  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    if (child->node == node)
      {
        g_assert (length <= child->length);
        child->length -= length;
        cjh_text_region_subtract_from_parents (region, parent, length);
        return;
      }
  });

  g_assert_not_reached ();
}

static void
cjh_text_region_add_to_parents (CjhTextRegion     *region,
                                CjhTextRegionNode *node,
                                gsize              length)
{
  CjhTextRegionNode *parent = cjh_text_region_node_get_parent (node);

  if (parent == NULL || length == 0)
    return;

  cjh_text_region_invalid_cache (region);

  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    if (child->node == node)
      {
        child->length += length;
        cjh_text_region_add_to_parents (region, parent, length);
        return;
      }
  });

  g_assert_not_reached ();
}

static inline gboolean
cjh_text_region_node_is_root (CjhTextRegionNode *node)
{
  return node != NULL && cjh_text_region_node_get_parent (node) == NULL;
}

static CjhTextRegionNode *
cjh_text_region_node_search_recurse (CjhTextRegionNode *node,
                                     gsize              offset,
                                     gsize             *offset_within_node)
{
  CjhTextRegionChild *last_child = NULL;

  g_assert (node != NULL);
  g_assert (offset_within_node != NULL);

  /* If we reached a leaf, that is all we need to do */
  if (cjh_text_region_node_is_leaf (node))
    {
      *offset_within_node = offset;
      return node;
    }

  g_assert (!cjh_text_region_node_is_leaf (node));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&node->branch.children));
  g_assert (offset <= cjh_text_region_node_length (node));

  SORTED_ARRAY_FOREACH (&node->branch.children, CjhTextRegionChild, child, {
    g_assert (child->length > 0);
    g_assert (child->node != NULL);

    if (offset < child->length)
      return cjh_text_region_node_search_recurse (child->node, offset, offset_within_node);

    offset -= child->length;
    last_child = child;
  });

  /* We're right-most, so it belongs at the end. Add back the length we removed
   * while trying to resolve within the parent branch.
   */
  g_assert (last_child != NULL);
  g_assert (node->branch.next == NULL);
  return cjh_text_region_node_search_recurse (last_child->node,
                                              offset + last_child->length,
                                              offset_within_node);
}

static CjhTextRegionNode *
cjh_text_region_search (CjhTextRegion *region,
                        gsize          offset,
                        gsize         *offset_within_node)
{
  CjhTextRegionNode *result;

  *offset_within_node = 0;

  g_assert (region->cached_result == NULL ||
            cjh_text_region_node_is_leaf (region->cached_result));

  /* Try to reuse cached node to avoid traversal since in most cases
   * an insert will be followed by another insert.
   */
  if (region->cached_result != NULL && offset >= region->cached_result_offset)
    {
      gsize calc_offset = region->cached_result_offset + cjh_text_region_node_length (region->cached_result);

      if (offset < calc_offset ||
          (offset == calc_offset && region->cached_result->leaf.next == NULL))
        {
          *offset_within_node = offset - region->cached_result_offset;
          return region->cached_result;
        }
    }

  if (offset == 0)
    result = _cjh_text_region_get_first_leaf (region);
  else
    result = cjh_text_region_node_search_recurse (&region->root, offset, offset_within_node);

  /* Now save it for cached reuse */
  if (result != NULL)
    {
      region->cached_result = result;
      region->cached_result_offset = offset - *offset_within_node;
    }

  return result;
}

static void
cjh_text_region_root_split (CjhTextRegion     *region,
                            CjhTextRegionNode *root)
{
  CjhTextRegionNode *left;
  CjhTextRegionNode *right;
  CjhTextRegionChild new_child;

  g_assert (region != NULL);
  g_assert (!cjh_text_region_node_is_leaf (root));
  g_assert (cjh_text_region_node_is_root (root));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&root->branch.children));

  left = cjh_text_region_node_new (root, FALSE);
  right = cjh_text_region_node_new (root, FALSE);

  left->branch.next = right;
  right->branch.prev = left;

  SORTED_ARRAY_SPLIT2 (&root->branch.children, &left->branch.children, &right->branch.children);
  SORTED_ARRAY_FOREACH (&left->branch.children, CjhTextRegionChild, child, {
    cjh_text_region_node_set_parent (child->node, left);
  });
  SORTED_ARRAY_FOREACH (&right->branch.children, CjhTextRegionChild, child, {
    cjh_text_region_node_set_parent (child->node, right);
  });

  g_assert (SORTED_ARRAY_IS_EMPTY (&root->branch.children));

  new_child.node = right;
  new_child.length = cjh_text_region_node_length (right);
  SORTED_ARRAY_PUSH_HEAD (&root->branch.children, new_child);

  new_child.node = left;
  new_child.length = cjh_text_region_node_length (left);
  SORTED_ARRAY_PUSH_HEAD (&root->branch.children, new_child);

  g_assert (SORTED_ARRAY_LENGTH (&root->branch.children) == 2);

  DEBUG_VALIDATE (root, NULL);
  DEBUG_VALIDATE (left, root);
  DEBUG_VALIDATE (right, root);
}

static CjhTextRegionNode *
cjh_text_region_branch_split (CjhTextRegion     *region,
                              CjhTextRegionNode *left)
{
  G_GNUC_UNUSED gsize old_length;
  CjhTextRegionNode *parent;
  CjhTextRegionNode *right;
  gsize right_length = 0;
  gsize left_length = 0;
  guint i = 0;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (!cjh_text_region_node_is_leaf (left));
  g_assert (!cjh_text_region_node_is_root (left));

  old_length = cjh_text_region_node_length (left);

  /*
   * This operation should not change the height of the tree. Only
   * splitting the root node can change the height of the tree. So
   * here we add a new right node, and update the parent to point to
   * it right after our node.
   *
   * Since no new items are added, lengths do not change and we do
   * not need to update lengths up the hierarchy except for our two
   * effected nodes (and their direct parent).
   */

  parent = cjh_text_region_node_get_parent (left);

  /* Create a new node to split half the items into */
  right = cjh_text_region_node_new (parent, FALSE);

  /* Insert node into branches linked list */
  right->branch.next = left->branch.next;
  right->branch.prev = left;
  if (right->branch.next != NULL)
    right->branch.next->branch.prev = right;
  left->branch.next = right;

  SORTED_ARRAY_SPLIT (&left->branch.children, &right->branch.children);
  SORTED_ARRAY_FOREACH (&right->branch.children, CjhTextRegionChild, child, {
    cjh_text_region_node_set_parent (child->node, right);
  });

#ifndef G_DISABLE_ASSERT
  SORTED_ARRAY_FOREACH (&left->branch.children, CjhTextRegionChild, child, {
    g_assert (cjh_text_region_node_get_parent (child->node) == left);
  });
#endif

  right_length = cjh_text_region_node_length (right);
  left_length = cjh_text_region_node_length (left);

  g_assert (right_length + left_length == old_length);
  g_assert (SORTED_ARRAY_LENGTH (&parent->branch.children) < SORTED_ARRAY_CAPACITY (&parent->branch.children));

  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    i++;

    if (child->node == left)
      {
        CjhTextRegionChild right_child;

        right_child.node = right;
        right_child.length = right_length;

        child->length = left_length;

        SORTED_ARRAY_INSERT_VAL (&parent->branch.children, i, right_child);

        DEBUG_VALIDATE (left, parent);
        DEBUG_VALIDATE (right, parent);
        DEBUG_VALIDATE (parent, cjh_text_region_node_get_parent (parent));

        return right;
      }
  });

  g_assert_not_reached ();
}

static CjhTextRegionNode *
cjh_text_region_leaf_split (CjhTextRegion     *region,
                            CjhTextRegionNode *left)
{
  G_GNUC_UNUSED gsize length;
  CjhTextRegionNode *parent;
  CjhTextRegionNode *right;
  gsize right_length;
  guint i;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (cjh_text_region_node_is_leaf (left));

  parent = cjh_text_region_node_get_parent (left);

  g_assert (parent != left);
  g_assert (!cjh_text_region_node_is_leaf (parent));
  g_assert (!SORTED_ARRAY_IS_EMPTY (&parent->branch.children));
  g_assert (!SORTED_ARRAY_IS_FULL (&parent->branch.children));

  length = cjh_text_region_node_length (left);

  g_assert (length > 0);

  DEBUG_VALIDATE (parent, cjh_text_region_node_get_parent (parent));
  DEBUG_VALIDATE (left, parent);

  right = cjh_text_region_node_new (parent, TRUE);

  SORTED_ARRAY_SPLIT (&left->leaf.runs, &right->leaf.runs);
  right_length = cjh_text_region_node_length (right);

  g_assert (length == right_length + cjh_text_region_node_length (left));
  g_assert (cjh_text_region_node_is_leaf (left));
  g_assert (cjh_text_region_node_is_leaf (right));

  i = 0;
  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    G_GNUC_UNUSED const CjhTextRegionChild *next = SORTED_ARRAY_FOREACH_PEEK (&parent->branch.children);

    ++i;

    g_assert (cjh_text_region_node_is_leaf (child->node));
    g_assert (next == NULL || cjh_text_region_node_is_leaf (next->node));

    if (child->node == left)
      {
        CjhTextRegionChild right_child;

        g_assert (child->length >= right_length);
        g_assert (next == NULL || left->leaf.next == next->node);

        if (left->leaf.next != NULL)
          left->leaf.next->leaf.prev = right;

        right->leaf.prev = left;
        right->leaf.next = left->leaf.next;
        left->leaf.next = right;

        right_child.node = right;
        right_child.length = right_length;

        child->length -= right_length;

        g_assert (child->length > 0);
        g_assert (right_child.length > 0);

        SORTED_ARRAY_INSERT_VAL (&parent->branch.children, i, right_child);

        g_assert (right != NULL);
        g_assert (cjh_text_region_node_is_leaf (right));
        g_assert (right->leaf.prev == left);
        g_assert (left->leaf.next == right);

        DEBUG_VALIDATE (left, parent);
        DEBUG_VALIDATE (right, parent);
        DEBUG_VALIDATE (parent, cjh_text_region_node_get_parent (parent));

        return right;
      }
  });

  g_assert_not_reached ();
}

static inline gboolean
cjh_text_region_node_needs_split (CjhTextRegionNode *node)
{
  /*
   * We want to split the tree node if there is not enough space to
   * split a single entry into two AND add a new entry. That means we
   * need two empty slots before we ever perform an insert.
   */

  if (!cjh_text_region_node_is_leaf (node))
    return SORTED_ARRAY_LENGTH (&node->branch.children) >= (SORTED_ARRAY_CAPACITY (&node->branch.children) - 2);
  else
    return SORTED_ARRAY_LENGTH (&node->leaf.runs) >= (SORTED_ARRAY_CAPACITY (&node->leaf.runs) - 2);
}

static inline CjhTextRegionNode *
cjh_text_region_node_split (CjhTextRegion     *region,
                            CjhTextRegionNode *node)
{
  CjhTextRegionNode *parent;

  g_assert (node != NULL);

  cjh_text_region_invalid_cache (region);

  parent = cjh_text_region_node_get_parent (node);

  if (parent != NULL &&
      cjh_text_region_node_needs_split (parent))
    cjh_text_region_node_split (region, parent);

  if (!cjh_text_region_node_is_leaf (node))
    {
      if (cjh_text_region_node_is_root (node))
        {
          cjh_text_region_root_split (region, node);
          return &region->root;
        }

      return cjh_text_region_branch_split (region, node);
    }
  else
    {
      return cjh_text_region_leaf_split (region, node);
    }
}

CjhTextRegion *
_cjh_text_region_new (CjhTextRegionJoinFunc  join_func,
                      CjhTextRegionSplitFunc split_func)
{
  CjhTextRegion *self;
  CjhTextRegionNode *leaf;
  CjhTextRegionChild child;

  self = g_new0 (CjhTextRegion, 1);
  self->length = 0;
  self->join_func = join_func;
  self->split_func = split_func;

  /* The B+Tree has a root node (a branch) and a single leaf
   * as a child to simplify how we do splits/rotations/etc.
   */
  leaf = cjh_text_region_node_new (&self->root, TRUE);

  child.node = leaf;
  child.length = 0;

  SORTED_ARRAY_INIT (&self->root.branch.children);
  SORTED_ARRAY_PUSH_HEAD (&self->root.branch.children, child);

  return self;
}

static void
cjh_text_region_node_free (CjhTextRegionNode *node)
{
  if (node == NULL)
    return;

  if (!cjh_text_region_node_is_leaf (node))
    {
      SORTED_ARRAY_FOREACH (&node->branch.children, CjhTextRegionChild, child, {
        cjh_text_region_node_free (child->node);
      });
    }

  g_free (node);
}

void
_cjh_text_region_free (CjhTextRegion *region)
{
  if (region != NULL)
    {
      g_assert (cjh_text_region_node_is_root (&region->root));
      g_assert (!SORTED_ARRAY_IS_EMPTY (&region->root.branch.children));

      SORTED_ARRAY_FOREACH (&region->root.branch.children, CjhTextRegionChild, child, {
        cjh_text_region_node_free (child->node);
      });

      g_free (region);
    }
}

static inline gboolean
join_run (CjhTextRegion          *region,
          gsize                   offset,
          const CjhTextRegionRun *left,
          const CjhTextRegionRun *right,
          CjhTextRegionRun       *joined)
{
  gboolean join;

  g_assert (region != NULL);
  g_assert (left != NULL);
  g_assert (right != NULL);
  g_assert (joined != NULL);

  if (region->join_func != NULL)
    join = region->join_func (offset, left, right);
  else
    join = FALSE;

  if (join)
    {
      joined->length = left->length + right->length;
      joined->data = left->data;

      return TRUE;
    }

  return FALSE;
}

void
_cjh_text_region_insert (CjhTextRegion *region,
                         gsize          offset,
                         gsize          length,
                         gpointer       data)
{
  CjhTextRegionRun to_insert = { length, data };
  CjhTextRegionNode *target;
  CjhTextRegionNode *node;
  CjhTextRegionNode *parent;
  gsize offset_within_node = offset;
  guint i;

  g_assert (region != NULL);
  g_assert (offset <= region->length);

  if (length == 0)
    return;

  target = cjh_text_region_search (region, offset, &offset_within_node);

  g_assert (cjh_text_region_node_is_leaf (target));
  g_assert (offset_within_node <= cjh_text_region_node_length (target));

  /* We should only hit this if we have an empty tree. */
  if G_UNLIKELY (SORTED_ARRAY_IS_EMPTY (&target->leaf.runs))
    {
      g_assert (offset == 0);
      SORTED_ARRAY_PUSH_HEAD (&target->leaf.runs, to_insert);
      g_assert (cjh_text_region_node_length (target) == length);
      goto inserted;
    }

  /* Split up to region->root if necessary */
  if (cjh_text_region_node_needs_split (target))
    {
      DEBUG_VALIDATE (target, cjh_text_region_node_get_parent (target));

      /* Split the target into two and then re-locate our position as
       * we might need to be in another node.
       *
       * TODO: Potentially optimization here to look at prev/next to
       *       locate which we need. Complicated though since we don't
       *       have real offsets.
       */
      cjh_text_region_node_split (region, target);

      target = cjh_text_region_search (region, offset, &offset_within_node);

      g_assert (cjh_text_region_node_is_leaf (target));
      g_assert (offset_within_node <= cjh_text_region_node_length (target));
      DEBUG_VALIDATE (target, cjh_text_region_node_get_parent (target));
    }

  i = 0;
  SORTED_ARRAY_FOREACH (&target->leaf.runs, CjhTextRegionRun, run, {
    /*
     * If this insert request would happen immediately after this run,
     * we want to see if we can chain it to this run or the beginning
     * of the next run.
     *
     * Note: We coudld also follow the the B+tree style linked-leaf to
     *       the next leaf and compare against it's first item. But that is
     *       out of scope for this prototype.
     */

    if (offset_within_node == 0)
      {
        if (!join_run (region, offset, &to_insert, run, run))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i, to_insert);
        goto inserted;
      }
    else if (offset_within_node == run->length)
      {
        CjhTextRegionRun *next = SORTED_ARRAY_FOREACH_PEEK (&target->leaf.runs);

        /* Try to chain to the end of this run or the beginning of the next */
        if (!join_run (region, offset, run, &to_insert, run) &&
            (next == NULL || !join_run (region, offset, &to_insert, next, next)))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, to_insert);
        goto inserted;
      }
    else if (offset_within_node < run->length)
      {
        CjhTextRegionRun left;
        CjhTextRegionRun right;

        left.length = offset_within_node;
        left.data = run->data;

        right.length = run->length - offset_within_node;
        right.data = run->data;

        cjh_text_region_split (region, offset - offset_within_node, run, &left, &right);

        *run = left;

        if (!join_run (region, offset, &to_insert, &right, &to_insert))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, right);

        if (!join_run (region, offset - offset_within_node, run, &to_insert, run))
          SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i + 1, to_insert);

        goto inserted;
      }

    offset_within_node -= run->length;

    i++;
  });

  g_assert_not_reached ();

inserted:

  g_assert (target != NULL);

  /*
   * Now update each of the parent nodes in the tree so that they have
   * an apprporiate length along with the child pointer. This allows them
   * to calculate offsets while walking the tree (without derefrencing the
   * child node) at the cost of us walking back up the tree.
   */
  for (parent = cjh_text_region_node_get_parent (target), node = target;
       parent != NULL;
       node = parent, parent = cjh_text_region_node_get_parent (node))
    {
      SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
        if (child->node == node)
          {
            child->length += length;
            goto found_in_parent;
          }
      });

      g_assert_not_reached ();

    found_in_parent:
      DEBUG_VALIDATE (node, parent);
      continue;
    }

  region->length += length;

  g_assert (region->length == cjh_text_region_node_length (&region->root));
}

void
_cjh_text_region_replace (CjhTextRegion *region,
                          gsize          offset,
                          gsize          length,
                          gpointer       data)
{
  g_assert (region != NULL);

  if (length == 0)
    return;

  /* TODO: This could be optimized to avoid possible splits
   *       by merging adjoining runs.
   */

  _cjh_text_region_remove (region, offset, length);
  _cjh_text_region_insert (region, offset, length, data);

  g_assert (region->length == cjh_text_region_node_length (&region->root));
}

guint
_cjh_text_region_get_length (CjhTextRegion *region)
{
  g_assert (region != NULL);

  return region->length;
}

static void
cjh_text_region_branch_compact (CjhTextRegion     *region,
                                CjhTextRegionNode *node)
{
  CjhTextRegionNode *parent;
  CjhTextRegionNode *left;
  CjhTextRegionNode *right;
  CjhTextRegionNode *target;
  gsize added = 0;
  gsize length;

  g_assert (region != NULL);
  g_assert (node != NULL);
  g_assert (!cjh_text_region_node_is_leaf (node));

  SORTED_ARRAY_FOREACH (&node->branch.children, CjhTextRegionChild, child, {
    if (child->node == NULL)
      {
        g_assert (child->length == 0);
        SORTED_ARRAY_FOREACH_REMOVE (&node->branch.children);
      }
  });

  if (cjh_text_region_node_is_root (node))
    return;

  parent = cjh_text_region_node_get_parent (node);

  g_assert (parent != NULL);
  g_assert (!cjh_text_region_node_is_leaf (parent));

  /* Reparent child in our stead if we can remove this node */
  if (SORTED_ARRAY_LENGTH (&node->branch.children) == 1 &&
      SORTED_ARRAY_LENGTH (&parent->branch.children) == 1)
    {
      CjhTextRegionChild *descendant = &SORTED_ARRAY_PEEK_HEAD (&node->branch.children);

      g_assert (parent->branch.prev == NULL);
      g_assert (parent->branch.next == NULL);
      g_assert (node->branch.prev == NULL);
      g_assert (node->branch.next == NULL);
      g_assert (descendant->node != NULL);

      SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
        if (child->node == node)
          {
            child->node = descendant->node;
            cjh_text_region_node_set_parent (child->node, parent);

            descendant->node = NULL;
            descendant->length = 0;

            goto compact_parent;
          }
      });

      g_assert_not_reached ();
    }

  if (node->branch.prev == NULL && node->branch.next == NULL)
    return;

  if (SORTED_ARRAY_LENGTH (&node->branch.children) >= CJH_TEXT_REGION_MIN_BRANCHES)
    return;

  length = cjh_text_region_node_length (node);
  cjh_text_region_subtract_from_parents (region, node, length);

  /* Remove this node, we'll reparent the children with edges */
  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    if (child->node == node)
      {
        SORTED_ARRAY_FOREACH_REMOVE (&parent->branch.children);
        goto found;
      }
  });

  g_assert_not_reached ();

found:
  left = node->branch.prev;
  right = node->branch.next;

  if (left != NULL)
    left->branch.next = right;

  if (right != NULL)
    right->branch.prev = left;

  if (left == NULL ||
      (right != NULL &&
       SORTED_ARRAY_LENGTH (&left->branch.children) > SORTED_ARRAY_LENGTH (&right->branch.children)))
    {
      target = right;

      g_assert (target->branch.prev == left);

      SORTED_ARRAY_FOREACH_REVERSE (&node->branch.children, CjhTextRegionChild, child, {
        if (SORTED_ARRAY_LENGTH (&target->branch.children) >= CJH_TEXT_REGION_MAX_BRANCHES-1)
          {
            cjh_text_region_add_to_parents (region, target, added);
            added = 0;
            cjh_text_region_branch_split (region, target);
            g_assert (target->branch.prev == left);
          }

        cjh_text_region_node_set_parent (child->node, target);
        added += child->length;
        SORTED_ARRAY_PUSH_HEAD (&target->branch.children, *child);

        child->node = NULL;
        child->length = 0;
      });

      cjh_text_region_add_to_parents (region, target, added);
    }
  else
    {
      target = left;

      g_assert (target->branch.next == right);

      SORTED_ARRAY_FOREACH (&node->branch.children, CjhTextRegionChild, child, {
        if (SORTED_ARRAY_LENGTH (&target->branch.children) >= CJH_TEXT_REGION_MAX_BRANCHES-1)
          {
            cjh_text_region_add_to_parents (region, target, added);
            added = 0;
            target = cjh_text_region_branch_split (region, target);
          }

        cjh_text_region_node_set_parent (child->node, target);
        added += child->length;
        SORTED_ARRAY_PUSH_TAIL (&target->branch.children, *child);

        child->node = NULL;
        child->length = 0;
      });

      cjh_text_region_add_to_parents (region, target, added);
    }

  DEBUG_VALIDATE (left, cjh_text_region_node_get_parent (left));
  DEBUG_VALIDATE (right, cjh_text_region_node_get_parent (right));
  DEBUG_VALIDATE (parent, cjh_text_region_node_get_parent (parent));

compact_parent:
  if (parent != NULL)
    cjh_text_region_branch_compact (region, parent);

  cjh_text_region_node_free (node);
}

static void
cjh_text_region_leaf_compact (CjhTextRegion     *region,
                              CjhTextRegionNode *node)
{
  CjhTextRegionNode *parent;
  CjhTextRegionNode *target;
  CjhTextRegionNode *left;
  CjhTextRegionNode *right;
  gsize added = 0;

  g_assert (region != NULL);
  g_assert (node != NULL);
  g_assert (cjh_text_region_node_is_leaf (node));
  g_assert (SORTED_ARRAY_LENGTH (&node->leaf.runs) < CJH_TEXT_REGION_MIN_RUNS);

  /* Short-circuit if we are the only node */
  if (node->leaf.prev == NULL && node->leaf.next == NULL)
    return;

  parent = cjh_text_region_node_get_parent (node);
  left = node->leaf.prev;
  right = node->leaf.next;

  g_assert (parent != NULL);
  g_assert (!cjh_text_region_node_is_leaf (parent));
  g_assert (left == NULL || cjh_text_region_node_is_leaf (left));
  g_assert (right == NULL || cjh_text_region_node_is_leaf (right));

  SORTED_ARRAY_FOREACH (&parent->branch.children, CjhTextRegionChild, child, {
    if (child->node == node)
      {
        cjh_text_region_subtract_from_parents (region, node, child->length);
        g_assert (child->length == 0);
        SORTED_ARRAY_FOREACH_REMOVE (&parent->branch.children);
        goto found;
      }
  });

  g_assert_not_reached ();

found:
  if (left != NULL)
    left->leaf.next = right;

  if (right != NULL)
    right->leaf.prev = left;

  node->leaf.next = NULL;
  node->leaf.prev = NULL;

  if (left == NULL ||
      (right != NULL &&
       SORTED_ARRAY_LENGTH (&left->leaf.runs) > SORTED_ARRAY_LENGTH (&right->leaf.runs)))
    {
      target = right;

      g_assert (target->leaf.prev == left);

      SORTED_ARRAY_FOREACH_REVERSE (&node->leaf.runs, CjhTextRegionRun, run, {
        if (SORTED_ARRAY_LENGTH (&target->leaf.runs) >= CJH_TEXT_REGION_MAX_RUNS-1)
          {
            cjh_text_region_add_to_parents (region, target, added);
            added = 0;
            cjh_text_region_node_split (region, target);
            g_assert (target->leaf.prev == left);
          }

        added += run->length;
        SORTED_ARRAY_PUSH_HEAD (&target->leaf.runs, *run);
      });

      cjh_text_region_add_to_parents (region, target, added);
    }
  else
    {
      target = left;

      g_assert (target->leaf.next == right);

      SORTED_ARRAY_FOREACH (&node->leaf.runs, CjhTextRegionRun, run, {
        if (SORTED_ARRAY_LENGTH (&target->leaf.runs) >= CJH_TEXT_REGION_MAX_RUNS-1)
          {
            cjh_text_region_add_to_parents (region, target, added);
            added = 0;

            target = cjh_text_region_node_split (region, target);

            left = target;
          }

        added += run->length;
        SORTED_ARRAY_PUSH_TAIL (&target->leaf.runs, *run);
      });

      cjh_text_region_add_to_parents (region, target, added);
    }

  DEBUG_VALIDATE (left, cjh_text_region_node_get_parent (left));
  DEBUG_VALIDATE (right, cjh_text_region_node_get_parent (right));
  DEBUG_VALIDATE (parent, cjh_text_region_node_get_parent (parent));

  cjh_text_region_branch_compact (region, parent);

  cjh_text_region_node_free (node);
}

void
_cjh_text_region_remove (CjhTextRegion *region,
                         gsize          offset,
                         gsize          length)
{
  CjhTextRegionNode *target;
  gsize offset_within_node;
  gsize to_remove = length;
  gsize calc_offset;
  guint i;

  g_assert (region != NULL);
  g_assert (length <= region->length);
  g_assert (offset < region->length);
  g_assert (length <= region->length - offset);

  if (length == 0)
    return;

  target = cjh_text_region_search (region, offset, &offset_within_node);

  g_assert (target != NULL);
  g_assert (cjh_text_region_node_is_leaf (target));
  g_assert (SORTED_ARRAY_LENGTH (&target->leaf.runs) > 0);
  g_assert (offset >= offset_within_node);

  calc_offset = offset - offset_within_node;

  i = 0;
  SORTED_ARRAY_FOREACH (&target->leaf.runs, CjhTextRegionRun, run, {
    ++i;

    g_assert (to_remove > 0);

    if (offset_within_node >= run->length)
      {
        offset_within_node -= run->length;
        calc_offset += run->length;
      }
    else if (offset_within_node > 0 && to_remove >= run->length - offset_within_node)
      {
        CjhTextRegionRun left;
        CjhTextRegionRun right;

        left.length = offset_within_node;
        left.data = run->data;
        right.length = run->length - left.length;
        right.data = run->data;
        cjh_text_region_split (region, calc_offset, run, &left, &right);

        to_remove -= right.length;
        calc_offset += left.length;
        offset_within_node = 0;

        *run = left;

        if (to_remove == 0)
          break;
      }
    else if (offset_within_node > 0 && to_remove < run->length - offset_within_node)
      {
        CjhTextRegionRun saved;
        CjhTextRegionRun left;
        CjhTextRegionRun right;
        CjhTextRegionRun right2;
        CjhTextRegionRun center;

        left.length = offset_within_node;
        left.data = run->data;
        right.length = run->length - left.length;
        right.data = run->data;
        cjh_text_region_split (region, calc_offset, run, &left, &right);

        center.length = to_remove;
        center.data = run->data;
        right2.length = run->length - offset_within_node - to_remove;
        right2.data = run->data;
        cjh_text_region_split (region, calc_offset + left.length, &right, &center, &right2);

        saved = *run;
        *run = left;

        if (!join_run (region, calc_offset, run, &right2, run))
          {
            if (!SORTED_ARRAY_IS_FULL (&target->leaf.runs))
              {
                /* If there is space in our sorted array for the additional
                 * split we have here, then go ahead and do that since it
                 * avoids re-entering the btree.
                 */
                SORTED_ARRAY_INSERT_VAL (&target->leaf.runs, i, right2);
              }
            else
              {
                /* Degenerate case in that our leaf is full. We could
                 * potentially peek at the next and push to the head there, but
                 * I'm avoiding that because it would amortize out with the
                 * split in place anyway.
                 */
                *run = saved;
                cjh_text_region_node_split (region, target);
                _cjh_text_region_remove (region, offset, length);
                break;
              }
          }

        offset_within_node = 0;
        to_remove = 0;

        break;
      }
    else if (offset_within_node == 0 && to_remove < run->length)
      {
        CjhTextRegionRun left;
        CjhTextRegionRun right;

        left.length = to_remove;
        left.data = run->data;

        right.length = run->length - to_remove;
        right.data = run->data;

        cjh_text_region_split (region, calc_offset, run, &left, &right);

        to_remove = 0;
        offset_within_node = 0;

        *run = right;

        break;
      }
    else if (offset_within_node == 0 && to_remove >= run->length)
      {
        to_remove -= run->length;

        SORTED_ARRAY_FOREACH_REMOVE (&target->leaf.runs);

        if (to_remove == 0)
          break;
      }
    else
      {
        g_assert_not_reached ();
      }

    g_assert (to_remove > 0);
  });

  region->length -= length - to_remove;
  cjh_text_region_subtract_from_parents (region, target, length - to_remove);

  if (SORTED_ARRAY_LENGTH (&target->leaf.runs) < CJH_TEXT_REGION_MIN_RUNS)
    cjh_text_region_leaf_compact (region, target);

  g_assert (region->length == cjh_text_region_node_length (&region->root));

  if (to_remove > 0)
    _cjh_text_region_remove (region, offset, to_remove);
}

void
_cjh_text_region_foreach (CjhTextRegion            *region,
                          CjhTextRegionForeachFunc  func,
                          gpointer                  user_data)
{
  CjhTextRegionNode *leaf;
  gsize offset = 0;

  g_return_if_fail (region != NULL);
  g_return_if_fail (func != NULL);

  for (leaf = _cjh_text_region_get_first_leaf (region);
       leaf != NULL;
       leaf = leaf->leaf.next)
    {
      g_assert (leaf->leaf.next == NULL || leaf->leaf.next->leaf.prev == leaf);

      SORTED_ARRAY_FOREACH (&leaf->leaf.runs, CjhTextRegionRun, run, {
        if (func (offset, run, user_data))
          return;
        offset += run->length;
      });
    }
}

void
_cjh_text_region_foreach_in_range (CjhTextRegion            *region,
                                   gsize                     begin,
                                   gsize                     end,
                                   CjhTextRegionForeachFunc  func,
                                   gpointer                  user_data)
{
  CjhTextRegionNode *leaf;
  gsize position;
  gsize offset_within_node = 0;

  g_return_if_fail (region != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (begin <= region->length);
  g_return_if_fail (end <= region->length);
  g_return_if_fail (begin <= end);

  if (begin == end || begin == region->length)
    return;

  if (begin == 0)
    leaf = _cjh_text_region_get_first_leaf (region);
  else
    leaf = cjh_text_region_search (region, begin, &offset_within_node);

  g_assert (offset_within_node < cjh_text_region_node_length (leaf));

  position = begin - offset_within_node;

  while (position < end)
    {
      SORTED_ARRAY_FOREACH (&leaf->leaf.runs, CjhTextRegionRun, run, {
        if (offset_within_node >= run->length)
          {
            offset_within_node -= run->length;
          }
        else
          {
            offset_within_node = 0;
            if (func (position, run, user_data))
              return;
          }

        position += run->length;

        if (position >= end)
          break;
      });

      leaf = leaf->leaf.next;
    }
}
