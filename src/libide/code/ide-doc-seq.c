/* ide-doc-seq.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-doc-seq"

#include "config.h"

#include "ide-doc-seq-private.h"

static GHashTable *seq;

guint
ide_doc_seq_acquire (void)
{
  guint seq_id;

  if (!seq)
    seq = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (seq_id = 1; seq_id < G_MAXUINT; seq_id++)
    {
      gpointer key;

      key = GINT_TO_POINTER (seq_id);

      if (!g_hash_table_lookup (seq, key))
        {
          g_hash_table_insert (seq, key, GINT_TO_POINTER (TRUE));
          return seq_id;
        }
    }

  return 0;
}

void
ide_doc_seq_release (guint seq_id)
{
  g_hash_table_remove (seq, GINT_TO_POINTER (seq_id));
}
