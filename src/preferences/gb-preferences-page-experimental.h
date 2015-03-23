/* gb-preferences-page-experimental.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_PREFERENCES_PAGE_EXPERIMENTAL_H
#define GB_PREFERENCES_PAGE_EXPERIMENTAL_H

#include "gb-preferences-page.h"

G_BEGIN_DECLS

#define GB_TYPE_PREFERENCES_PAGE_EXPERIMENTAL (gb_preferences_page_experimental_get_type())

G_DECLARE_FINAL_TYPE (GbPreferencesPageExperimental, gb_preferences_page_experimental,
                      GB, PREFERENCES_PAGE_EXPERIMENTAL, GbPreferencesPage)

G_END_DECLS

#endif /* GB_PREFERENCES_PAGE_EXPERIMENTAL_H */
