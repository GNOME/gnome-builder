/* ide-tree-node.h
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

#ifndef IDE_TREE_NODE_H
#define IDE_TREE_NODE_H

#include "ide-tree-types.h"

G_BEGIN_DECLS

IdeTreeNode    *ide_tree_node_new                   (void);
void           ide_tree_node_append                (IdeTreeNode   *node,
                                                   IdeTreeNode   *child);
void           ide_tree_node_insert_sorted         (IdeTreeNode   *node,
                                                   IdeTreeNode   *child,
                                                   IdeTreeNodeCompareFunc compare_func,
                                                   gpointer      user_data);
const gchar   *ide_tree_node_get_icon_name         (IdeTreeNode   *node);
GObject       *ide_tree_node_get_item              (IdeTreeNode   *node);
IdeTreeNode    *ide_tree_node_get_parent            (IdeTreeNode   *node);
GtkTreePath   *ide_tree_node_get_path              (IdeTreeNode   *node);
gboolean       ide_tree_node_get_iter              (IdeTreeNode   *node,
                                                   GtkTreeIter  *iter);
void           ide_tree_node_prepend               (IdeTreeNode   *node,
                                                   IdeTreeNode   *child);
void           ide_tree_node_remove                (IdeTreeNode   *node,
                                                   IdeTreeNode   *child);
void           ide_tree_node_set_icon_name         (IdeTreeNode   *node,
                                                   const gchar  *icon_name);
void           ide_tree_node_set_item              (IdeTreeNode   *node,
                                                   GObject      *item);
gboolean       ide_tree_node_expand                (IdeTreeNode   *node,
                                                   gboolean      expand_ancestors);
void           ide_tree_node_collapse              (IdeTreeNode   *node);
void           ide_tree_node_select                (IdeTreeNode   *node);
void           ide_tree_node_get_area              (IdeTreeNode   *node,
                                                   GdkRectangle *area);
void           ide_tree_node_invalidate            (IdeTreeNode   *node);
gboolean       ide_tree_node_get_expanded          (IdeTreeNode   *node);
void           ide_tree_node_show_popover          (IdeTreeNode   *node,
                                                   GtkPopover   *popover);
const gchar   *ide_tree_node_get_text              (IdeTreeNode   *node);
void           ide_tree_node_set_text              (IdeTreeNode   *node,
                                                   const gchar  *text);
IdeTree        *ide_tree_node_get_tree              (IdeTreeNode   *node);
gboolean       ide_tree_node_get_children_possible (IdeTreeNode   *self);
void           ide_tree_node_set_children_possible (IdeTreeNode   *self,
                                                   gboolean      children_possible);
gboolean       ide_tree_node_get_use_markup        (IdeTreeNode   *self);
void           ide_tree_node_set_use_markup        (IdeTreeNode   *self,
                                                   gboolean      use_markup);
gboolean       ide_tree_node_get_use_dim_label     (IdeTreeNode   *self);
void           ide_tree_node_set_use_dim_label     (IdeTreeNode   *self,
                                                   gboolean      use_dim_label);

G_END_DECLS

#endif /* IDE_TREE_NODE_H */
