/* gb-tree-node.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#ifndef GB_TREE_NODE_H
#define GB_TREE_NODE_H

#include "gb-tree-types.h"

G_BEGIN_DECLS

GbTreeNode    *gb_tree_node_new           (void);
void           gb_tree_node_append        (GbTreeNode   *node,
                                           GbTreeNode   *child);
void           gb_tree_node_insert_sorted (GbTreeNode   *node,
                                           GbTreeNode   *child,
                                           GbTreeNodeCompareFunc compare_func,
                                           gpointer      user_data);
const gchar   *gb_tree_node_get_icon_name (GbTreeNode   *node);
GObject       *gb_tree_node_get_item      (GbTreeNode   *node);
GbTreeNode    *gb_tree_node_get_parent    (GbTreeNode   *node);
GtkTreePath   *gb_tree_node_get_path      (GbTreeNode   *node);
gboolean       gb_tree_node_get_iter      (GbTreeNode   *node,
                                           GtkTreeIter  *iter);
void           gb_tree_node_prepend       (GbTreeNode   *node,
                                           GbTreeNode   *child);
void           gb_tree_node_remove        (GbTreeNode   *node,
                                           GbTreeNode   *child);
void           gb_tree_node_set_icon_name (GbTreeNode   *node,
                                           const gchar  *icon_name);
void           gb_tree_node_set_item      (GbTreeNode   *node,
                                           GObject      *item);
void           gb_tree_node_expand        (GbTreeNode   *node,
                                           gboolean      expand_ancestors);
void           gb_tree_node_collapse      (GbTreeNode   *node);
void           gb_tree_node_select        (GbTreeNode   *node);
void           gb_tree_node_get_area      (GbTreeNode   *node,
                                           GdkRectangle *area);
void           gb_tree_node_invalidate    (GbTreeNode   *node);
gboolean       gb_tree_node_get_expanded  (GbTreeNode   *node);
void           gb_tree_node_show_popover  (GbTreeNode   *node,
                                           GtkPopover   *popover);
const gchar   *gb_tree_node_get_text      (GbTreeNode   *node);
void           gb_tree_node_set_text      (GbTreeNode   *node,
                                           const gchar  *text);
GbTree        *gb_tree_node_get_tree      (GbTreeNode   *node);
void           gb_tree_node_set_children_possible
                                          (GbTreeNode   *self,
                                           gboolean      children_possible);

G_END_DECLS

#endif /* GB_TREE_NODE_H */
