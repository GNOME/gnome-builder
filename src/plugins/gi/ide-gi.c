/* ide-gi.c
 *
 * Copyright © 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include <dazzle.h>

#include "ide-gi.h"

void
ide_gi_global_index_entry_clear (gpointer data)
{
  IdeGiGlobalIndexEntry *entry = (IdeGiGlobalIndexEntry *)data;

  g_return_if_fail (data != NULL);

  dzl_clear_pointer (&entry->name, g_free);
}
