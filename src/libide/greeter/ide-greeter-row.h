/* ide-greeter-row.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GREETER_INSIDE) && !defined (IDE_GREETER_COMPILATION)
# error "Only <libide-greeter.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-projects.h>

G_BEGIN_DECLS

#define IDE_TYPE_GREETER_ROW (ide_greeter_row_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeGreeterRow, ide_greeter_row, IDE, GREETER_ROW, GtkListBoxRow)

struct _IdeGreeterRowClass
{
  GtkListBoxRowClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdeGreeterRow  *ide_greeter_row_new                (void);
IDE_AVAILABLE_IN_ALL
IdeProjectInfo *ide_greeter_row_get_project_info   (IdeGreeterRow  *self);
IDE_AVAILABLE_IN_ALL
void            ide_greeter_row_set_project_info   (IdeGreeterRow  *self,
                                                    IdeProjectInfo *project_info);
IDE_AVAILABLE_IN_ALL
gchar          *ide_greeter_row_get_search_text    (IdeGreeterRow  *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_greeter_row_get_selection_mode (IdeGreeterRow  *self);
IDE_AVAILABLE_IN_ALL
void            ide_greeter_row_set_selection_mode (IdeGreeterRow  *self,
                                                    gboolean        selection_mode);

G_END_DECLS
