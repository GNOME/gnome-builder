/* ide-greeter-project-row.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_RECENT_PROJECT_ROW (gbp_recent_project_row_get_type())

G_DECLARE_FINAL_TYPE (GbpRecentProjectRow, gbp_recent_project_row, GBP, RECENT_PROJECT_ROW, GtkListBoxRow)

IdeProjectInfo *gbp_recent_project_row_get_project_info   (GbpRecentProjectRow *self);
const gchar    *gbp_recent_project_row_get_search_text    (GbpRecentProjectRow *self);
void            gbp_recent_project_row_set_selection_mode (GbpRecentProjectRow *self,
                                                           gboolean             selection_mode);

G_END_DECLS
