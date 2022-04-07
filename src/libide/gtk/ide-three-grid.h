/* ide-three-grid.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_THREE_GRID (ide_three_grid_get_type())

typedef enum
{
  IDE_THREE_GRID_COLUMN_LEFT,
  IDE_THREE_GRID_COLUMN_CENTER,
  IDE_THREE_GRID_COLUMN_RIGHT
} IdeThreeGridColumn;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeThreeGrid, ide_three_grid, IDE, THREE_GRID, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_three_grid_new             (void);
IDE_AVAILABLE_IN_ALL
void       ide_three_grid_add             (IdeThreeGrid       *self,
                                           GtkWidget          *child,
                                           guint               row,
                                           IdeThreeGridColumn  column);
IDE_AVAILABLE_IN_ALL
void       ide_three_grid_remove          (IdeThreeGrid       *self,
                                           GtkWidget          *child);

G_END_DECLS
