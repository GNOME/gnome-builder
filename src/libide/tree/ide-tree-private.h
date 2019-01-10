/* ide-tree-private.h
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

#include "ide-tree.h"
#include "ide-tree-node.h"
#include "ide-tree-model.h"

G_BEGIN_DECLS

GdkDragAction _ide_tree_get_drop_actions              (IdeTree         *tree);
IdeTreeModel *_ide_tree_model_new                     (IdeTree         *tree);
IdeTreeNode  *_ide_tree_get_drop_node                 (IdeTree         *tree);
void          _ide_tree_model_release_addins          (IdeTreeModel    *self);
void          _ide_tree_model_selection_changed       (IdeTreeModel    *model,
                                                       GtkTreeIter     *selection);
void          _ide_tree_model_build_node              (IdeTreeModel    *self,
                                                       IdeTreeNode     *node);
gboolean      _ide_tree_model_row_activated           (IdeTreeModel    *self,
                                                       IdeTree         *tree,
                                                       GtkTreePath     *path);
void          _ide_tree_model_row_expanded            (IdeTreeModel    *self,
                                                       IdeTree         *tree,
                                                       GtkTreePath     *path);
void          _ide_tree_model_row_collapsed           (IdeTreeModel    *self,
                                                       IdeTree         *tree,
                                                       GtkTreePath     *path);
void          _ide_tree_model_cell_data_func          (IdeTreeModel    *self,
                                                       GtkTreeIter     *iter,
                                                       GtkCellRenderer *cell);
gboolean      _ide_tree_model_contains_node           (IdeTreeModel    *self,
                                                       IdeTreeNode     *node);
gboolean      _ide_tree_node_get_loading              (IdeTreeNode     *self,
                                                       gint64          *loading_started_at);
void          _ide_tree_node_set_loading              (IdeTreeNode     *self,
                                                       gboolean         loading);
void          _ide_tree_node_dump                     (IdeTreeNode     *self);
void          _ide_tree_node_remove_all               (IdeTreeNode     *self);
void          _ide_tree_node_set_model                (IdeTreeNode     *self,
                                                       IdeTreeModel    *model);
gboolean      _ide_tree_node_get_needs_build_children (IdeTreeNode     *self);
void          _ide_tree_node_set_needs_build_children (IdeTreeNode     *self,
                                                       gboolean         needs_build_children);
void          _ide_tree_node_show_popover             (IdeTreeNode     *node,
                                                       IdeTree         *tree,
                                                       GtkPopover      *popover);
GIcon        *_ide_tree_node_apply_emblems            (IdeTreeNode     *self,
                                                       GIcon           *base);
void          _ide_tree_node_apply_colors             (IdeTreeNode     *self,
                                                       GtkCellRenderer *cell);

G_END_DECLS
