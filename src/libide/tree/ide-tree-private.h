/* ide-tree-private.h
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

GtkTreeListRow *_ide_tree_get_row_at_node      (IdeTree                 *self,
                                                IdeTreeNode             *node,
                                                gboolean                 expand_to_row);
gboolean        _ide_tree_node_children_built  (IdeTreeNode             *self);
guint           _ide_tree_node_get_child_index (IdeTreeNode             *parent,
                                                IdeTreeNode             *child);
IdeTree        *_ide_tree_node_get_tree        (IdeTreeNode             *self);
void            _ide_tree_node_collapsed       (IdeTreeNode             *self);
DexFuture      *_ide_tree_node_expand          (IdeTreeNode             *self,
                                                GListModel              *addins);
gboolean        _ide_tree_node_show_popover    (IdeTreeNode             *self,
                                                GtkPopover              *popover);

G_END_DECLS
