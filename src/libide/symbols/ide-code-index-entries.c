/* ide-code-index-entries.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-entries"

#include "application/ide-application.h"
#include "symbols/ide-code-index-entries.h"

G_DEFINE_INTERFACE (IdeCodeIndexEntries, ide_code_index_entries, G_TYPE_OBJECT)

static void
ide_code_index_entries_default_init (IdeCodeIndexEntriesInterface *iface)
{
}

/**
 * ide_code_index_entries_get_next_entry:
 * @self: An #IdeCodeIndexEntries instance.
 *
 * This will fetch next entry in index.
 *
 * When all of the entries have been exhausted, %NULL should be returned.
 *
 * Returns: (nullable) (transfer full): An #IdeCodeIndexEntry.
 *
 * Since: 3.26
 */
IdeCodeIndexEntry *
ide_code_index_entries_get_next_entry (IdeCodeIndexEntries *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRIES (self), NULL);

  return IDE_CODE_INDEX_ENTRIES_GET_IFACE (self)->get_next_entry (self);
}

/**
 * ide_code_index_entries_get_file:
 * @self: a #IdeCodeIndexEntries
 *
 * The file that was indexed.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
ide_code_index_entries_get_file (IdeCodeIndexEntries *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_ENTRIES (self), NULL);

  return IDE_CODE_INDEX_ENTRIES_GET_IFACE (self)->get_file (self);
}
