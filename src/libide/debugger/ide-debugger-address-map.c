/* ide-debugger-address-map.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-address-map"

#include "config.h"

#include "ide-debugger-address-map-private.h"

struct _IdeDebuggerAddressMap
{
  GSequence    *seq;
  GStringChunk *chunk;
};

static inline gint
uint64cmp (guint64 a,
           guint64 b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

static gint
ide_debugger_address_map_entry_compare (gconstpointer a,
                                        gconstpointer b,
                                        gpointer      user_data)
{
  const IdeDebuggerAddressMapEntry *entry_a = a;
  const IdeDebuggerAddressMapEntry *entry_b = b;

  return uint64cmp (entry_a->start, entry_b->start);
}

static gint
ide_debugger_address_map_entry_compare_in_range (gconstpointer a,
                                                 gconstpointer b,
                                                 gpointer      user_data)
{
  const IdeDebuggerAddressMapEntry *entry_a = a;
  const IdeDebuggerAddressMapEntry *entry_b = b;

  /*
   * entry_b is the needle for the search.
   * Only entry_b->start is set.
   */

  if ((entry_b->start >= entry_a->start) && (entry_b->start < entry_a->end))
    return 0;

  return uint64cmp (entry_a->start, entry_b->start);
}

static void
ide_debugger_address_map_entry_free (gpointer data)
{
  IdeDebuggerAddressMapEntry *entry = data;

  if (entry != NULL)
    g_slice_free (IdeDebuggerAddressMapEntry, entry);
}

/**
 * ide_debugger_address_map:
 *
 * Creates a new #IdeDebuggerAddressMap.
 *
 * The map is used to track the locations of mapped files in the inferiors
 * address space. This allows relatively quick lookup to determine what file
 * contains a given execution address (instruction pointer, etc).
 *
 * See also: ide_debugger_address_map_free()
 *
 * Returns: (transfer full): A new #IdeDebuggerAddressMap
 */
IdeDebuggerAddressMap *
ide_debugger_address_map_new (void)
{
  IdeDebuggerAddressMap *ret;

  ret = g_slice_new0 (IdeDebuggerAddressMap);
  ret->seq = g_sequence_new (ide_debugger_address_map_entry_free);
  ret->chunk = g_string_chunk_new (4096);

  return ret;
}

/**
 * ide_debugger_address_map_free:
 * @self: a #IdeDebuggerAddressMap
 *
 * Frees all memory associated with @self.
 */
void
ide_debugger_address_map_free (IdeDebuggerAddressMap *self)
{
  if (self != NULL)
    {
      g_sequence_free (self->seq);
      g_string_chunk_free (self->chunk);
      g_slice_free (IdeDebuggerAddressMap, self);
    }
}

/**
 * ide_debugger_address_map_insert:
 * @self: a #IdeDebuggerAddressMap
 * @map: the map entry to insert
 *
 * Inserts a new map entry as specified by @entry.
 *
 * The contents of @entry are copied and therefore do not need to be kept
 * around after calling this function.
 *
 * See also: ide_debugger_address_map_remove()
 */
void
ide_debugger_address_map_insert (IdeDebuggerAddressMap            *self,
                                 const IdeDebuggerAddressMapEntry *entry)
{
  IdeDebuggerAddressMapEntry real = { 0 };

  g_return_if_fail (self != NULL);
  g_return_if_fail (entry != NULL);

  real.filename = g_string_chunk_insert_const (self->chunk, entry->filename);
  real.start = entry->start;
  real.end = entry->end;
  real.offset = entry->offset;

  g_sequence_insert_sorted (self->seq,
                            g_slice_dup (IdeDebuggerAddressMapEntry, &real),
                            ide_debugger_address_map_entry_compare,
                            NULL);
}

/**
 * ide_debugger_address_map_lookup:
 * @self: a #IdeDebuggerAddressMap
 * @address: an address to locate the containing map
 *
 * Attempts to locate which #IdeDebuggerAddressMapEntry contains @address within
 * the region specified by #IdeDebuggerAddressMapEntry.start and
 * #IdeDebuggerAddressMapEntry.end.
 *
 * Returns: (nullable): An #IdeDebuggerAddressMapEntry or %NULL
 */
const IdeDebuggerAddressMapEntry *
ide_debugger_address_map_lookup (const IdeDebuggerAddressMap *self,
                                 guint64                      address)
{
  IdeDebuggerAddressMapEntry entry = { NULL, 0, address, 0 };
  GSequenceIter *iter;

  g_return_val_if_fail (self != NULL, NULL);

  iter = g_sequence_lookup (self->seq,
                            &entry,
                            ide_debugger_address_map_entry_compare_in_range,
                            NULL);

  if (iter == NULL || g_sequence_iter_is_end (iter))
    return NULL;

  return g_sequence_get (iter);
}

/**
 * ide_debugger_address_map_remove:
 * @self: a #IdeDebuggerAddressMap
 * @address: the address contained in the map
 *
 * Removes the entry found containing @address.
 */
gboolean
ide_debugger_address_map_remove (IdeDebuggerAddressMap *self,
                                 IdeDebuggerAddress     address)
{
  IdeDebuggerAddressMapEntry entry = { NULL, 0, address, 0 };
  GSequenceIter *iter;

  g_return_val_if_fail (self != NULL, FALSE);

  iter = g_sequence_lookup (self->seq,
                            &entry,
                            ide_debugger_address_map_entry_compare_in_range,
                            NULL);

  if (iter == NULL || g_sequence_iter_is_end (iter))
    return FALSE;

  g_sequence_remove (iter);

  return TRUE;
}
