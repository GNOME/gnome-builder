/*
 * code-sparse-set.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_CONST
static inline gboolean
_ispow2 (guint v)
{
  return (v != 0) && ((v & (v - 1)) == 0);
}

typedef struct _CodeSparseSetItem
{
  guint value;
  guint user_value;
} CodeSparseSetItem;

typedef struct _CodeSparseSet
{
  CodeSparseSetItem *dense;
  guint             *sparse;
  guint              capacity;
  guint              len;
} CodeSparseSet;

#define CODE_SPARSE_SET_INIT(max) \
  (CodeSparseSet) { \
    .dense = g_new (CodeSparseSetItem, 8), \
    .sparse = g_new (guint, max), \
    .capacity = max, \
    .len = 0, \
  }

static inline void
code_sparse_set_init (CodeSparseSet *sparse_set,
                      guint          max)
{
  *sparse_set = CODE_SPARSE_SET_INIT (max);
}

static inline void
code_sparse_set_clear (CodeSparseSet *sparse_set)
{
  g_clear_pointer (&sparse_set->dense, g_free);
  g_clear_pointer (&sparse_set->sparse, g_free);
  sparse_set->len = 0;
  sparse_set->capacity = 0;
}

static inline void
code_sparse_set_reset (CodeSparseSet *sparse_set)
{
  if (sparse_set->len != 0)
    {
      sparse_set->dense = g_renew (CodeSparseSetItem, sparse_set->dense, 8);
      sparse_set->len = 0;
    }
}

static inline gboolean
code_sparse_set_add_with_data (CodeSparseSet *sparse_set,
                               guint          value,
                               guint          user_value)
{
  guint idx;

  g_return_val_if_fail (value < sparse_set->capacity, FALSE);

  idx = sparse_set->sparse[value];

  if (idx < sparse_set->len && sparse_set->dense[idx].value == value)
    return FALSE;

  idx = sparse_set->len;
  sparse_set->dense[idx].value = value;
  sparse_set->dense[idx].user_value = user_value;
  sparse_set->sparse[value] = idx;

  sparse_set->len++;

  if (sparse_set->len < 8)
    return TRUE;

  if (_ispow2 (sparse_set->len))
    sparse_set->dense = g_renew (CodeSparseSetItem, sparse_set->dense, sparse_set->len * 2);

  return TRUE;
}

static inline gboolean
code_sparse_set_add (CodeSparseSet *sparse_set,
                     guint          value)
{
  return code_sparse_set_add_with_data (sparse_set, value, 0);
}

static inline gboolean
code_sparse_set_contains (CodeSparseSet *sparse_set,
                          guint          value)
{
  guint idx;

  if (value >= sparse_set->capacity)
    return FALSE;

  idx = sparse_set->sparse[value];

  return idx < sparse_set->len && sparse_set->dense[idx].value == value;
}

static inline gboolean
code_sparse_set_get (CodeSparseSet *sparse_set,
                     guint          value,
                     guint         *user_value)
{
  guint idx;

  if (value >= sparse_set->capacity)
    return FALSE;

  idx = sparse_set->sparse[value];

  if (idx < sparse_set->len && sparse_set->dense[idx].value == value)
    {
      *user_value = sparse_set->dense[idx].user_value;
      return TRUE;
    }

  return FALSE;
}

static inline int
_code_sparse_set_compare (gconstpointer          a,
                          gconstpointer          b,
                          G_GNUC_UNUSED gpointer data)
{
  const CodeSparseSetItem *aval = a;
  const CodeSparseSetItem *bval = b;

  if (aval->value < bval->value)
    return -1;
  else if (aval->value > bval->value)
    return 1;
  else
    return 0;
}

static inline void
code_sparse_set_sort (CodeSparseSet *sparse_set)
{
  if (sparse_set->len < 2)
    return;

  g_sort_array (sparse_set->dense,
                sparse_set->len,
                sizeof sparse_set->dense[0],
                _code_sparse_set_compare,
                NULL);

  for (guint i = 0; i < sparse_set->len; i++)
    sparse_set->sparse[sparse_set->dense[i].value] = i;
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (CodeSparseSet, code_sparse_set_clear)

G_END_DECLS
