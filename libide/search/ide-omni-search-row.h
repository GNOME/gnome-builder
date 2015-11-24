/* ide-omni-search-row.h
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

#ifndef IDE_OMNI_SEARCH_ROW_H
#define IDE_OMNI_SEARCH_ROW_H

#include <gtk/gtk.h>

#include "ide-search-result.h"

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_SEARCH_ROW (ide_omni_search_row_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniSearchRow, ide_omni_search_row, IDE, OMNI_SEARCH_ROW, GtkListBoxRow)

IdeSearchResult *ide_omni_search_row_get_result (IdeOmniSearchRow *row);
void             ide_omni_search_row_set_result (IdeOmniSearchRow *row,
                                                 IdeSearchResult  *result);

G_END_DECLS

#endif /* IDE_OMNI_SEARCH_ROW_H */
