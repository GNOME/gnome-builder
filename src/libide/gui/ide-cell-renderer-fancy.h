/* ide-cell-renderer-fancy.h
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

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_CELL_RENDERER_FANCY (ide_cell_renderer_fancy_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeCellRendererFancy, ide_cell_renderer_fancy, IDE, CELL_RENDERER_FANCY, GtkCellRenderer)

IDE_AVAILABLE_IN_3_32
GtkCellRenderer *ide_cell_renderer_fancy_new        (void);
IDE_AVAILABLE_IN_3_32
const gchar     *ide_cell_renderer_fancy_get_body   (IdeCellRendererFancy *self);
IDE_AVAILABLE_IN_3_32
const gchar     *ide_cell_renderer_fancy_get_title  (IdeCellRendererFancy *self);
IDE_AVAILABLE_IN_3_32
void             ide_cell_renderer_fancy_take_title (IdeCellRendererFancy *self,
                                                     gchar                *title);
IDE_AVAILABLE_IN_3_32
void             ide_cell_renderer_fancy_set_title  (IdeCellRendererFancy *self,
                                                     const gchar          *title);
IDE_AVAILABLE_IN_3_32
void             ide_cell_renderer_fancy_set_body   (IdeCellRendererFancy *self,
                                                     const gchar          *body);

G_END_DECLS
