/* ide-tree.h
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

#ifndef IDE_TREE_H
#define IDE_TREE_H

#include <gtk/gtk.h>

#include "ide-tree-builder.h"
#include "ide-tree-node.h"
#include "ide-tree-types.h"

G_BEGIN_DECLS

/**
 * IdeTreeFindFunc:
 *
 * Callback to check @child, a child of @node, matches a lookup
 * request. Returns %TRUE if @child matches, %FALSE if not.
 *
 * Returns: %TRUE if @child matched
 */
typedef gboolean (*IdeTreeFindFunc) (IdeTree     *tree,
                                    IdeTreeNode *node,
                                    IdeTreeNode *child,
                                    gpointer    user_data);

/**
 * IdeTreeFilterFunc:
 *
 * Callback to check if @node should be visible.
 *
 * Returns: %TRUE if @node should be visible.
 */
typedef gboolean (*IdeTreeFilterFunc) (IdeTree     *tree,
                                      IdeTreeNode *node,
                                      gpointer    user_data);

struct _IdeTreeClass
{
	GtkTreeViewClass parent_class;

  void (*action)         (IdeTree      *self,
                          const gchar *action_group,
                          const gchar *action_name,
                          const gchar *param);
  void (*populate_popup) (IdeTree      *self,
                          GtkWidget   *widget);
};

void          ide_tree_add_builder     (IdeTree           *self,
                                       IdeTreeBuilder    *builder);
void          ide_tree_remove_builder  (IdeTree           *self,
                                       IdeTreeBuilder    *builder);
IdeTreeNode   *ide_tree_find_item       (IdeTree           *self,
                                       GObject          *item);
IdeTreeNode   *ide_tree_find_custom     (IdeTree           *self,
                                       GEqualFunc        equal_func,
                                       gpointer          key);
IdeTreeNode   *ide_tree_get_selected    (IdeTree           *self);
void          ide_tree_rebuild         (IdeTree           *self);
void          ide_tree_set_root        (IdeTree           *self,
                                       IdeTreeNode       *node);
IdeTreeNode   *ide_tree_get_root        (IdeTree           *self);
void          ide_tree_set_show_icons  (IdeTree           *self,
                                       gboolean          show_icons);
gboolean      ide_tree_get_show_icons  (IdeTree           *self);
void          ide_tree_scroll_to_node  (IdeTree           *self,
                                       IdeTreeNode       *node);
void          ide_tree_expand_to_node  (IdeTree           *self,
                                       IdeTreeNode       *node);
IdeTreeNode   *ide_tree_find_child_node (IdeTree           *self,
                                       IdeTreeNode       *node,
                                       IdeTreeFindFunc    find_func,
                                       gpointer          user_data);
void          ide_tree_set_filter      (IdeTree           *self,
                                       IdeTreeFilterFunc  filter_func,
                                       gpointer          filter_data,
                                       GDestroyNotify    filter_data_destroy);

G_END_DECLS

#endif /* IDE_TREE_H */
