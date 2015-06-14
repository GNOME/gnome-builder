/* gb-tree.h
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

#ifndef GB_TREE_H
#define GB_TREE_H

#include <gtk/gtk.h>

#include "gb-tree-builder.h"
#include "gb-tree-node.h"
#include "gb-tree-types.h"

G_BEGIN_DECLS

/**
 * GbTreeFindFunc:
 *
 * Callback to check @child, a child of @node, matches a lookup
 * request. Returns %TRUE if @child matches, %FALSE if not.
 *
 * Returns: %TRUE if @child matched
 */
typedef gboolean (*GbTreeFindFunc) (GbTree     *tree,
                                    GbTreeNode *node,
                                    GbTreeNode *child,
                                    gpointer    user_data);

struct _GbTreeClass
{
	GtkTreeViewClass parent_class;

  void (*action)         (GbTree      *self,
                          const gchar *action_group,
                          const gchar *action_name,
                          const gchar *param);
  void (*populate_popup) (GbTree      *self,
                          GtkWidget   *widget);
};

void          gb_tree_add_builder    (GbTree        *self,
                                      GbTreeBuilder *builder);
void          gb_tree_remove_builder (GbTree        *self,
                                      GbTreeBuilder *builder);
GbTreeNode   *gb_tree_find_item      (GbTree        *self,
                                      GObject       *item);
GbTreeNode   *gb_tree_find_custom    (GbTree        *self,
                                      GEqualFunc     equal_func,
                                      gpointer       key);
GbTreeNode   *gb_tree_get_selected   (GbTree        *self);
void          gb_tree_rebuild        (GbTree        *self);
void          gb_tree_append         (GbTree        *self,
                                      GbTreeNode    *node,
                                      GbTreeNode    *child);
void          gb_tree_prepend        (GbTree        *self,
                                      GbTreeNode    *node,
                                      GbTreeNode    *child);
void          gb_tree_set_root       (GbTree        *self,
                                      GbTreeNode    *node);
GbTreeNode   *gb_tree_get_root       (GbTree        *self);
void          gb_tree_set_show_icons (GbTree        *self,
                                      gboolean       show_icons);
gboolean      gb_tree_get_show_icons (GbTree        *self);
void          gb_tree_scroll_to_node (GbTree        *self,
                                      GbTreeNode    *node);
void          gb_tree_expand_to_node (GbTree        *self,
                                      GbTreeNode    *node);
GbTreeNode   *gb_tree_find_child_node (GbTree         *self,
                                       GbTreeNode     *node,
                                       GbTreeFindFunc  find_func,
                                       gpointer        user_data);

G_END_DECLS

#endif /* GB_TREE_H */
