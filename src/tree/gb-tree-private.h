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

#include "gb-tree.h"
#include "gb-tree-builder.h"
#include "gb-tree-node.h"

G_BEGIN_DECLS

void _gb_tree_rebuild_node  (GbTree     *tree,
                             GbTreeNode *node);
void _gb_tree_node_set_tree (GbTreeNode *node,
                             GbTree     *tree);

G_END_DECLS

#endif /* GB_TREE_PRIVATE_H */
