/* ide-preferences-group.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_PREFERENCES_GROUP_H
#define IDE_PREFERENCES_GROUP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_GROUP (ide_preferences_group_get_type())

G_DECLARE_FINAL_TYPE (IdePreferencesGroup, ide_preferences_group, IDE, PREFERENCES_GROUP, GtkBin)

void ide_preferences_group_add (IdePreferencesGroup *self,
                                GtkWidget           *widget);

G_END_DECLS

#endif /* IDE_PREFERENCES_GROUP_H */
