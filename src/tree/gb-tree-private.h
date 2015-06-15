/* gb-tree-private.h
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

#ifndef GB_TREE_PRIVATE_H
#define GB_TREE_PRIVATE_H

#include "gb-tree-types.h"

G_BEGIN_DECLS

void         _gb_tree_invalidate              (GbTree        *tree,
                                               GbTreeNode    *node);
GtkTreePath *_gb_tree_get_path                (GbTree        *tree,
                                               GList         *list);
void         _gb_tree_append                  (GbTree        *self,
                                               GbTreeNode    *node,
                                               GbTreeNode    *child);
void         _gb_tree_prepend                 (GbTree        *self,
                                               GbTreeNode    *node,
                                               GbTreeNode    *child);
void         _gb_tree_insert_sorted           (GbTree        *self,
                                               GbTreeNode    *node,
                                               GbTreeNode    *child,
                                               GbTreeNodeCompareFunc compare_func,
                                               gpointer       user_data);

void         _gb_tree_node_set_tree           (GbTreeNode    *node,
                                               GbTree        *tree);
void         _gb_tree_node_set_parent         (GbTreeNode    *node,
                                               GbTreeNode    *parent);
gboolean     _gb_tree_node_get_needs_build    (GbTreeNode    *node);
void         _gb_tree_node_set_needs_build    (GbTreeNode    *node,
                                               gboolean       needs_build);
void         _gb_tree_node_remove_dummy_child (GbTreeNode    *node);

void         _gb_tree_builder_set_tree        (GbTreeBuilder *builder,
                                               GbTree        *tree);
void         _gb_tree_builder_added           (GbTreeBuilder *builder,
                                               GbTree        *tree);
void         _gb_tree_builder_removed         (GbTreeBuilder *builder,
                                               GbTree        *tree);
void         _gb_tree_builder_build_node      (GbTreeBuilder *builder,
                                               GbTreeNode    *node);
gboolean     _gb_tree_builder_node_activated  (GbTreeBuilder *builder,
                                               GbTreeNode    *node);
void         _gb_tree_builder_node_popup      (GbTreeBuilder *builder,
                                               GbTreeNode    *node,
                                               GMenu         *menu);
void         _gb_tree_builder_node_selected   (GbTreeBuilder *builder,
                                               GbTreeNode    *node);
void         _gb_tree_builder_node_unselected (GbTreeBuilder *builder,
                                               GbTreeNode    *node);

G_END_DECLS

#endif /* GB_TREE_PRIVATE_H */
