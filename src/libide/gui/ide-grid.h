/* ide-grid.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libpanel.h>

#include <libide-core.h>

#include "ide-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_GRID (ide_grid_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeGrid, ide_grid, IDE, GRID, PanelGrid)

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_grid_new               (void);
IDE_AVAILABLE_IN_ALL
guint      ide_grid_count_pages       (IdeGrid         *self);
IDE_AVAILABLE_IN_ALL
void       ide_grid_get_page_position (IdeGrid         *self,
                                       IdePage         *page,
                                       guint           *column,
                                       guint           *row,
                                       guint           *depth);
IDE_AVAILABLE_IN_ALL
void        ide_grid_foreach_page     (IdeGrid         *self,
                                       IdePageCallback  callback,
                                       gpointer         user_data);
IDE_AVAILABLE_IN_ALL
IdeFrame   *ide_grid_make_frame       (IdeGrid         *self,
                                       guint            column,
                                       guint            row);

G_END_DECLS
