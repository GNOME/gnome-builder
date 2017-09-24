/* ide-greeter-project-row.h
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

#pragma once

#include <gtk/gtk.h>

#include "projects/ide-project-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_GREETER_PROJECT_ROW (ide_greeter_project_row_get_type())

G_DECLARE_FINAL_TYPE (IdeGreeterProjectRow, ide_greeter_project_row, IDE, GREETER_PROJECT_ROW, GtkListBoxRow)

IdeProjectInfo *ide_greeter_project_row_get_project_info   (IdeGreeterProjectRow *self);
const gchar    *ide_greeter_project_row_get_search_text    (IdeGreeterProjectRow *self);
void            ide_greeter_project_row_set_selection_mode (IdeGreeterProjectRow *self,
                                                            gboolean              selection_mode);

G_END_DECLS
