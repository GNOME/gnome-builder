/* ide-layout-grid-column.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>

#include "layout/ide-layout-stack.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_GRID_COLUMN (ide_layout_grid_column_get_type())

G_DECLARE_FINAL_TYPE (IdeLayoutGridColumn, ide_layout_grid_column, IDE, LAYOUT_GRID_COLUMN, DzlMultiPaned)

GtkWidget      *ide_layout_grid_column_new               (void);
IdeLayoutStack *ide_layout_grid_column_get_current_stack (IdeLayoutGridColumn *self);
void            ide_layout_grid_column_set_current_stack (IdeLayoutGridColumn *self,
                                                          IdeLayoutStack      *stack);

G_END_DECLS
