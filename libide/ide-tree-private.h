/* ide-tree-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_TREE_PRIVATE_H
#define IDE_TREE_PRIVATE_H

#include "ide-tree-types.h"

G_BEGIN_DECLS

void         _ide_tree_invalidate              (IdeTree        *tree,
                                                IdeTreeNode    *node);
GtkTreePath *_ide_tree_get_path                (IdeTree        *tree,
                                                GList         *list);
void         _ide_tree_build_node              (IdeTree        *self,
                                                IdeTreeNode    *node);
void         _ide_tree_append                  (IdeTree        *self,
                                                IdeTreeNode    *node,
                                                IdeTreeNode    *child);
void         _ide_tree_prepend                 (IdeTree        *self,
                                                IdeTreeNode    *node,
                                                IdeTreeNode    *child);
void         _ide_tree_insert_sorted           (IdeTree        *self,
                                                IdeTreeNode    *node,
                                                IdeTreeNode    *child,
                                                IdeTreeNodeCompareFunc compare_func,
                                                gpointer        user_data);
void         _ide_tree_remove                  (IdeTree        *self,
                                                IdeTreeNode    *node);
gboolean     _ide_tree_get_iter                (IdeTree        *self,
                                                IdeTreeNode    *node,
                                                GtkTreeIter    *iter);
GtkTreeStore*_ide_tree_get_store               (IdeTree        *self);

void         _ide_tree_node_set_tree           (IdeTreeNode    *node,
                                                IdeTree        *tree);
void         _ide_tree_node_set_parent         (IdeTreeNode    *node,
                                                IdeTreeNode    *parent);
gboolean     _ide_tree_node_get_needs_build    (IdeTreeNode    *node);
void         _ide_tree_node_set_needs_build    (IdeTreeNode    *node,
                                                gboolean        needs_build);
void         _ide_tree_node_remove_dummy_child (IdeTreeNode    *node);

void         _ide_tree_builder_set_tree        (IdeTreeBuilder *builder,
                                                IdeTree        *tree);
void         _ide_tree_builder_added           (IdeTreeBuilder *builder,
                                                IdeTree        *tree);
void         _ide_tree_builder_removed         (IdeTreeBuilder *builder,
                                                IdeTree        *tree);
void         _ide_tree_builder_build_node      (IdeTreeBuilder *builder,
                                                IdeTreeNode    *node);
gboolean     _ide_tree_builder_node_activated  (IdeTreeBuilder *builder,
                                                IdeTreeNode    *node);
void         _ide_tree_builder_node_popup      (IdeTreeBuilder *builder,
                                                IdeTreeNode    *node,
                                                GMenu          *menu);
void         _ide_tree_builder_node_selected   (IdeTreeBuilder *builder,
                                                IdeTreeNode    *node);
void         _ide_tree_builder_node_unselected (IdeTreeBuilder *builder,
                                                IdeTreeNode    *node);

G_END_DECLS

#endif /* IDE_TREE_PRIVATE_H */
