/* ide-preferences-group-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_PREFERENCES_GROUP_PRIVATE_H
#define IDE_PREFERENCES_GROUP_PRIVATE_H

#include "ide-preferences-group.h"

G_BEGIN_DECLS

void _ide_preferences_group_set_map (IdePreferencesGroup *self,
                                     GHashTable          *map);

G_END_DECLS

#endif /* IDE_PREFERENCES_GROUP_PRIVATE_H */
