/* gb-document-grid.h
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_DOCUMENT_GRID_H
#define GB_DOCUMENT_GRID_H

#include <gtk/gtk.h>

#include "gb-document.h"
#include "gb-view-stack.h"

G_BEGIN_DECLS

#define GB_TYPE_VIEW_GRID (gb_view_grid_get_type())

G_DECLARE_FINAL_TYPE (GbViewGrid, gb_view_grid, GB, VIEW_GRID, GtkBin)

GtkWidget *gb_view_grid_new                  (void);
GtkWidget *gb_view_grid_add_stack_after      (GbViewGrid  *grid,
                                              GbViewStack *stack);
GtkWidget *gb_view_grid_add_stack_before     (GbViewGrid  *grid,
                                              GbViewStack *stack);
GtkWidget *gb_view_grid_get_stack_after      (GbViewGrid  *grid,
                                              GbViewStack *stack);
GtkWidget *gb_view_grid_get_stack_before     (GbViewGrid  *grid,
                                              GbViewStack *stack);
GList     *gb_view_grid_get_stacks           (GbViewGrid  *grid);
void       gb_view_grid_focus_document       (GbViewGrid  *grid,
                                              GbDocument  *document);

G_END_DECLS

#endif /* GB_DOCUMENT_GRID_H */
