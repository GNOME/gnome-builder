/* gb-preferences-page-theme.h
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

#ifndef GB_PREFERENCES_PAGE_THEME_H
#define GB_PREFERENCES_PAGE_THEME_H

#include "gb-preferences-page.h"

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_PAGE_THEME (gb_preferences_page_theme_get_type())

G_DECLARE_FINAL_TYPE (GbPreferencesPageTheme, gb_preferences_page_theme,
                      GB, PREFERENCES_PAGE_THEME, GbPreferencesPage)

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_THEME_H */
