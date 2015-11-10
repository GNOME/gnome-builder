/* ide-preferences-font-button.h
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

#ifndef IDE_PREFERENCES_FONT_BUTTON_H
#define IDE_PREFERENCES_FONT_BUTTON_H

#include "ide-preferences-bin.h"

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_FONT_BUTTON (ide_preferences_font_button_get_type())

G_DECLARE_FINAL_TYPE (IdePreferencesFontButton, ide_preferences_font_button, IDE, PREFERENCES_FONT_BUTTON, IdePreferencesBin)

G_END_DECLS

#endif /* IDE_PREFERENCES_FONT_BUTTON_H */
