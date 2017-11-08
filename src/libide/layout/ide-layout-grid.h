/* ide-layout-grid.h
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

#include "ide-version-macros.h"

#include "layout/ide-layout-grid-column.h"
#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_LAYOUT_GRID (ide_layout_grid_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeLayoutGrid, ide_layout_grid, IDE, LAYOUT_GRID, DzlMultiPaned)

struct _IdeLayoutGridClass
{
  DzlMultiPanedClass parent_class;

  IdeLayoutStack *(*create_stack) (IdeLayoutGrid *self);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_ALL
GtkWidget           *ide_layout_grid_new                (void);
IDE_AVAILABLE_IN_ALL
IdeLayoutGridColumn *ide_layout_grid_get_nth_column     (IdeLayoutGrid       *self,
                                                         gint                 nth);
IDE_AVAILABLE_IN_ALL
IdeLayoutView       *ide_layout_grid_focus_neighbor     (IdeLayoutGrid       *self,
                                                         GtkDirectionType     dir);
IDE_AVAILABLE_IN_ALL
IdeLayoutGridColumn *ide_layout_grid_get_current_column (IdeLayoutGrid       *self);
IDE_AVAILABLE_IN_ALL
void                 ide_layout_grid_set_current_column (IdeLayoutGrid       *self,
                                                         IdeLayoutGridColumn *column);
IDE_AVAILABLE_IN_ALL
IdeLayoutStack      *ide_layout_grid_get_current_stack  (IdeLayoutGrid       *self);
IDE_AVAILABLE_IN_ALL
IdeLayoutView       *ide_layout_grid_get_current_view   (IdeLayoutGrid       *self);
IDE_AVAILABLE_IN_ALL
guint                ide_layout_grid_count_views        (IdeLayoutGrid       *self);
IDE_AVAILABLE_IN_ALL
void                 ide_layout_grid_foreach_view       (IdeLayoutGrid       *self,
                                                         GtkCallback          callback,
                                                         gpointer             user_data);

G_END_DECLS
