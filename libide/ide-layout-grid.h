/* ide-layout-grid.h
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

#ifndef IDE_LAYOUT_GRID_H
#define IDE_LAYOUT_GRID_H

#include <gtk/gtk.h>

#include "ide-layout-stack.h"
#include "ide-layout-stack-split.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_GRID (ide_layout_grid_get_type())

G_DECLARE_FINAL_TYPE (IdeLayoutGrid, ide_layout_grid, IDE, LAYOUT_GRID, GtkBin)

GtkWidget  *ide_layout_grid_new              (void);
GtkWidget  *ide_layout_grid_add_stack_after  (IdeLayoutGrid  *grid,
                                              IdeLayoutStack *stack);
GtkWidget  *ide_layout_grid_add_stack_before (IdeLayoutGrid  *grid,
                                              IdeLayoutStack *stack);
GtkWidget  *ide_layout_grid_get_stack_after  (IdeLayoutGrid  *grid,
                                              IdeLayoutStack *stack);
GtkWidget  *ide_layout_grid_get_stack_before (IdeLayoutGrid  *grid,
                                              IdeLayoutStack *stack);
GList      *ide_layout_grid_get_stacks       (IdeLayoutGrid  *grid);
GtkWidget  *ide_layout_grid_get_last_focus   (IdeLayoutGrid  *self);
void        ide_layout_grid_foreach_view     (IdeLayoutGrid  *self,
                                              GtkCallback     callback,
                                              gpointer        user_data);

G_END_DECLS

#endif /* IDE_LAYOUT_GRID_H */
