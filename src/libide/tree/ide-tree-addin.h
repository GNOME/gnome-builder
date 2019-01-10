/* ide-tree-addin.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#include "ide-tree.h"
#include "ide-tree-model.h"
#include "ide-tree-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_TREE_ADDIN (ide_tree_addin_get_type ())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeTreeAddin, ide_tree_addin, IDE, TREE_ADDIN, GObject)

struct _IdeTreeAddinInterface
{
  GTypeInterface parent;

  void     (*load)                  (IdeTreeAddin         *self,
                                     IdeTree              *tree,
                                     IdeTreeModel         *model);
  void     (*unload)                (IdeTreeAddin         *self,
                                     IdeTree              *tree,
                                     IdeTreeModel         *model);
  void     (*build_node)            (IdeTreeAddin         *self,
                                     IdeTreeNode          *node);
  void     (*build_children)        (IdeTreeAddin         *self,
                                     IdeTreeNode          *node);
  void     (*build_children_async)  (IdeTreeAddin         *self,
                                     IdeTreeNode          *node,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
  gboolean (*build_children_finish) (IdeTreeAddin         *self,
                                     GAsyncResult         *result,
                                     GError              **error);
  void     (*cell_data_func)        (IdeTreeAddin         *self,
                                     IdeTreeNode          *node,
                                     GtkCellRenderer      *cell);
  gboolean (*node_activated)        (IdeTreeAddin         *self,
                                     IdeTree              *tree,
                                     IdeTreeNode          *node);
  void     (*selection_changed)     (IdeTreeAddin         *self,
                                     IdeTreeNode          *selection);
  void     (*node_expanded)         (IdeTreeAddin         *self,
                                     IdeTreeNode          *node);
  void     (*node_collapsed)        (IdeTreeAddin         *self,
                                     IdeTreeNode          *node);
  gboolean (*node_draggable)        (IdeTreeAddin         *self,
                                     IdeTreeNode          *node);
  gboolean (*node_droppable)        (IdeTreeAddin         *self,
                                     IdeTreeNode          *drag_node,
                                     IdeTreeNode          *drop_node,
                                     GtkSelectionData     *selection);
  void     (*node_dropped_async)    (IdeTreeAddin         *self,
                                     IdeTreeNode          *drag_node,
                                     IdeTreeNode          *drop_node,
                                     GtkSelectionData     *selection,
                                     GdkDragAction         actions,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
  gboolean (*node_dropped_finish)   (IdeTreeAddin         *self,
                                     GAsyncResult         *result,
                                     GError              **error);
};

IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_load                  (IdeTreeAddin         *self,
                                               IdeTree              *tree,
                                               IdeTreeModel         *model);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_unload                (IdeTreeAddin         *self,
                                               IdeTree              *tree,
                                               IdeTreeModel         *model);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_build_node            (IdeTreeAddin         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_build_children_async  (IdeTreeAddin         *self,
                                               IdeTreeNode          *node,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean ide_tree_addin_build_children_finish (IdeTreeAddin         *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_3_32
gboolean ide_tree_addin_node_activated        (IdeTreeAddin         *self,
                                               IdeTree              *tree,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_selection_changed     (IdeTreeAddin         *self,
                                               IdeTreeNode          *selection);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_node_expanded         (IdeTreeAddin         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_node_collapsed        (IdeTreeAddin         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
gboolean ide_tree_addin_node_draggable        (IdeTreeAddin         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
gboolean ide_tree_addin_node_droppable        (IdeTreeAddin         *self,
                                               IdeTreeNode          *drag_node,
                                               IdeTreeNode          *drop_node,
                                               GtkSelectionData     *selection);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_node_dropped_async    (IdeTreeAddin         *self,
                                               IdeTreeNode          *drag_node,
                                               IdeTreeNode          *drop_node,
                                               GtkSelectionData     *selection,
                                               GdkDragAction         actions,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean ide_tree_addin_node_dropped_finish   (IdeTreeAddin         *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_3_32
void     ide_tree_addin_cell_data_func        (IdeTreeAddin         *self,
                                               IdeTreeNode          *node,
                                               GtkCellRenderer      *cell);

G_END_DECLS
