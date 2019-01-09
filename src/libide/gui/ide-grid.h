/* ide-grid.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <dazzle.h>
#include <libide-core.h>

#include "ide-grid-column.h"
#include "ide-frame.h"
#include "ide-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_GRID (ide_grid_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeGrid, ide_grid, IDE, GRID, DzlMultiPaned)

struct _IdeGridClass
{
  DzlMultiPanedClass parent_class;

  IdeFrame *(*create_frame) (IdeGrid     *self);
  IdePage  *(*create_page)  (IdeGrid     *self,
                             const gchar *uri);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget     *ide_grid_new                (void);
IDE_AVAILABLE_IN_3_32
IdeGridColumn *ide_grid_get_nth_column     (IdeGrid          *self,
                                            gint              nth);
IDE_AVAILABLE_IN_3_32
IdePage       *ide_grid_focus_neighbor     (IdeGrid          *self,
                                            GtkDirectionType  dir);
IDE_AVAILABLE_IN_3_32
IdeGridColumn *ide_grid_get_current_column (IdeGrid          *self);
IDE_AVAILABLE_IN_3_32
void           ide_grid_set_current_column (IdeGrid          *self,
                                            IdeGridColumn    *column);
IDE_AVAILABLE_IN_3_32
IdeFrame      *ide_grid_get_current_stack  (IdeGrid          *self);
IDE_AVAILABLE_IN_3_32
IdePage       *ide_grid_get_current_page   (IdeGrid          *self);
IDE_AVAILABLE_IN_3_32
guint          ide_grid_count_pages        (IdeGrid          *self);
IDE_AVAILABLE_IN_3_32
void           ide_grid_foreach_page       (IdeGrid          *self,
                                            GtkCallback       callback,
                                            gpointer          user_data);

G_END_DECLS
