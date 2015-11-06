/* ide-preferences-switch.h
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

#ifndef IDE_PREFERENCES_SWITCH_H
#define IDE_PREFERENCES_SWITCH_H

#include "ide-preferences-container.h"

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_SWITCH (ide_preferences_switch_get_type())

G_DECLARE_FINAL_TYPE (IdePreferencesSwitch, ide_preferences_switch, IDE, PREFERENCES_SWITCH, IdePreferencesContainer)

G_END_DECLS

#endif /* IDE_PREFERENCES_SWITCH_H */
