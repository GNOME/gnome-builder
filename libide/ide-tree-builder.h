/* ide-tree-builder.h
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

#ifndef IDE_TREE_BUILDER_H
#define IDE_TREE_BUILDER_H

#include <glib-object.h>

#include "ide-tree-node.h"
#include "ide-tree-types.h"

G_BEGIN_DECLS

struct _IdeTreeBuilderClass
{
  GInitiallyUnownedClass parent_class;

  void     (*added)           (IdeTreeBuilder *builder,
                               GtkWidget     *tree);
  void     (*removed)         (IdeTreeBuilder *builder,
                               GtkWidget     *tree);
  void     (*build_node)      (IdeTreeBuilder *builder,
                               IdeTreeNode    *node);
  gboolean (*node_activated)  (IdeTreeBuilder *builder,
                               IdeTreeNode    *node);
  void     (*node_selected)   (IdeTreeBuilder *builder,
                               IdeTreeNode    *node);
  void     (*node_unselected) (IdeTreeBuilder *builder,
                               IdeTreeNode    *node);
  void     (*node_popup)      (IdeTreeBuilder *builder,
                               IdeTreeNode    *node,
                               GMenu         *menu);
};

IdeTree *ide_tree_builder_get_tree (IdeTreeBuilder *builder);

G_END_DECLS

#endif /* IDE_TREE_BUILDER_H */
