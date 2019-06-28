/* ide-tree-node.h
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

G_BEGIN_DECLS

#define IDE_TYPE_TREE_NODE (ide_tree_node_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeTreeNode, ide_tree_node, IDE, TREE_NODE, GObject)

typedef enum
{
  IDE_TREE_NODE_VISIT_BREAK    = 0,
  IDE_TREE_NODE_VISIT_CONTINUE = 0x1,
  IDE_TREE_NODE_VISIT_CHILDREN = 0x3,
} IdeTreeNodeVisit;

typedef enum
{
  IDE_TREE_NODE_FLAGS_NONE       = 0,
  IDE_TREE_NODE_FLAGS_DESCENDANT = 1 << 0,
  IDE_TREE_NODE_FLAGS_ADDED      = 1 << 1,
  IDE_TREE_NODE_FLAGS_CHANGED    = 1 << 2,
  IDE_TREE_NODE_FLAGS_REMOVED    = 1 << 3,
} IdeTreeNodeFlags;

/**
 * IdeTreeTraverseFunc:
 * @node: an #IdeTreeNode
 * @user_data: closure data provided to ide_tree_node_traverse()
 *
 * This function prototype is used to traverse a tree of #IdeTreeNode.
 *
 * Returns: #IdeTreeNodeVisit, %IDE_TREE_NODE_VISIT_BREAK to stop traversal.
 *
 * Since: 3.32
 */
typedef IdeTreeNodeVisit (*IdeTreeTraverseFunc) (IdeTreeNode *node,
                                                 gpointer     user_data);


/**
 * IdeTreeNodeCompare:
 * @node: an #IdeTreeNode that iterate over children
 * @child: an #IdeTreeNode to be inserted
 *

 * This callback function is a convenience wrapper around GCompareFunc
 *
 * Returns: int
 *
 * Since: 3.32
 */
typedef int (*IdeTreeNodeCompare) (IdeTreeNode *node,
                                   IdeTreeNode *child);

IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_new                    (void);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_get_has_error          (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_has_error          (IdeTreeNode         *self,
                                                     gboolean             has_error);
IDE_AVAILABLE_IN_3_32
const gchar   *ide_tree_node_get_tag                (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_tag                (IdeTreeNode         *self,
                                                     const gchar         *tag);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_tag                 (IdeTreeNode         *self,
                                                     const gchar         *tag);
IDE_AVAILABLE_IN_3_32
GtkTreePath   *ide_tree_node_get_path               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
const gchar   *ide_tree_node_get_display_name       (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_display_name       (IdeTreeNode         *self,
                                                     const gchar         *display_name);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_get_is_header          (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_is_header          (IdeTreeNode         *self,
                                                     gboolean             header);
IDE_AVAILABLE_IN_3_32
GIcon         *ide_tree_node_get_icon               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_icon               (IdeTreeNode         *self,
                                                     GIcon               *icon);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_icon_name          (IdeTreeNode         *self,
                                                     const gchar         *icon_name);
IDE_AVAILABLE_IN_3_32
GIcon         *ide_tree_node_get_expanded_icon      (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_expanded_icon      (IdeTreeNode         *self,
                                                     GIcon               *expanded_icon);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_expanded_icon_name (IdeTreeNode         *self,
                                                     const gchar         *expanded_icon_name);
IDE_AVAILABLE_IN_3_32
gpointer       ide_tree_node_get_item               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_item               (IdeTreeNode         *self,
                                                     gpointer             item);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_get_children_possible  (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_children_possible  (IdeTreeNode         *self,
                                                     gboolean             children_possible);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_empty               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_has_child              (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
guint          ide_tree_node_get_n_children         (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_get_next               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_get_previous           (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
guint          ide_tree_node_get_index              (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_get_nth_child          (IdeTreeNode         *self,
                                                     guint                index_);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_prepend                (IdeTreeNode         *self,
                                                     IdeTreeNode         *child);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_append                 (IdeTreeNode         *self,
                                                     IdeTreeNode         *child);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_insert_sorted          (IdeTreeNode         *self,
                                                     IdeTreeNode         *child,
                                                     IdeTreeNodeCompare   cmpfn);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_insert_before          (IdeTreeNode         *self,
                                                     IdeTreeNode         *child);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_insert_after           (IdeTreeNode         *self,
                                                     IdeTreeNode         *child);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_remove                 (IdeTreeNode         *self,
                                                     IdeTreeNode         *child);
IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_get_parent             (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_root                (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_first               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_last                (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
IdeTreeNode   *ide_tree_node_get_root               (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_holds                  (IdeTreeNode         *self,
                                                     GType                type);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_traverse               (IdeTreeNode         *self,
                                                     GTraverseType        traverse_type,
                                                     GTraverseFlags       traverse_flags,
                                                     gint                 max_depth,
                                                     IdeTreeTraverseFunc  traverse_func,
                                                     gpointer             user_data);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_add_emblem             (IdeTreeNode         *self,
                                                     GEmblem             *emblem);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_get_reset_on_collapse  (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_reset_on_collapse  (IdeTreeNode         *self,
                                                     gboolean             reset_on_collapse);
IDE_AVAILABLE_IN_3_32
const GdkRGBA *ide_tree_node_get_background_rgba    (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_background_rgba    (IdeTreeNode         *self,
                                                     const GdkRGBA       *background_rgba);
IDE_AVAILABLE_IN_3_32
const GdkRGBA *ide_tree_node_get_foreground_rgba    (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_foreground_rgba    (IdeTreeNode         *self,
                                                     const GdkRGBA       *foreground_rgba);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_is_selected            (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
gboolean       ide_tree_node_get_use_markup         (IdeTreeNode         *self);
IDE_AVAILABLE_IN_3_32
void           ide_tree_node_set_use_markup         (IdeTreeNode         *self,
                                                     gboolean             use_markup);
IDE_AVAILABLE_IN_3_34
void           ide_tree_node_set_flags              (IdeTreeNode         *self,
                                                     IdeTreeNodeFlags     flags);
IDE_AVAILABLE_IN_3_34
IdeTreeNodeFlags ide_tree_node_get_flags (IdeTreeNode *self);

G_END_DECLS
