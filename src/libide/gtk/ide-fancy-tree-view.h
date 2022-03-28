/* ide-fancy-tree-view.h
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

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_FANCY_TREE_VIEW (ide_fancy_tree_view_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeFancyTreeView, ide_fancy_tree_view, IDE, FANCY_TREE_VIEW, GtkTreeView)

struct _IdeFancyTreeViewClass
{
  GtkTreeViewClass parent_class;
};

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_fancy_tree_view_new           (void);
IDE_AVAILABLE_IN_ALL
void       ide_fancy_tree_view_set_data_func (IdeFancyTreeView      *self,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         func_data_destroy);

G_END_DECLS
