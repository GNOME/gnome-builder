/* ide-gi-complete-item.c
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

#include "ide-gi-complete-item.h"

#include <dazzle.h>
void
ide_gi_complete_prefix_item_clear (IdeGiCompletePrefixItem *self)
{
  g_return_if_fail (self != NULL);

  dzl_clear_pointer (&self->word, g_free);
  dzl_clear_pointer (&self->ns, ide_gi_namespace_unref);
}

void
ide_gi_complete_object_item_clear (IdeGiCompleteObjectItem *self)
{
  g_return_if_fail (self != NULL);

  dzl_clear_pointer (&self->word, g_free);
  dzl_clear_pointer (&self->object, ide_gi_base_unref);
}

void
ide_gi_complete_gtype_item_clear (IdeGiCompleteGtypeItem *self)
{
  g_return_if_fail (self != NULL);

  dzl_clear_pointer (&self->word, g_free);
  dzl_clear_pointer (&self->ns, ide_gi_namespace_unref);
}
