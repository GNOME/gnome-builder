/* ide-tree-types.h
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

#ifndef IDE_TREE_TYPES_H
#define IDE_TREE_TYPES_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_TREE         (ide_tree_get_type())
#define IDE_TYPE_TREE_NODE    (ide_tree_node_get_type())
#define IDE_TYPE_TREE_BUILDER (ide_tree_builder_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeTree,        ide_tree,         IDE, TREE,         GtkTreeView)
G_DECLARE_DERIVABLE_TYPE (IdeTreeBuilder, ide_tree_builder, IDE, TREE_BUILDER, GInitiallyUnowned)
G_DECLARE_FINAL_TYPE     (IdeTreeNode,    ide_tree_node,    IDE, TREE_NODE,    GInitiallyUnowned)

typedef gint (*IdeTreeNodeCompareFunc) (IdeTreeNode *a,
                                        IdeTreeNode *b,
                                        gpointer     user_data);

G_END_DECLS

#endif /* IDE_TREE_TYPES_H */
