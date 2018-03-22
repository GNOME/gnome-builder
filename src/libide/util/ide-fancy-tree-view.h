/* ide-fancy-tree-view.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_FANCY_TREE_VIEW (ide_fancy_tree_view_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeFancyTreeView, ide_fancy_tree_view, IDE, FANCY_TREE_VIEW, GtkTreeView)

struct _IdeFancyTreeViewClass
{
  GtkTreeViewClass parent_class;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_fancy_tree_view_new           (void);
IDE_AVAILABLE_IN_ALL
void       ide_fancy_tree_view_set_data_func (IdeFancyTreeView      *self,
                                              GtkCellLayoutDataFunc  func,
                                              gpointer               func_data,
                                              GDestroyNotify         func_data_destroy);

G_END_DECLS
