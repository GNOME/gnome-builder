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

G_BEGIN_DECLS

#define GB_TYPE_TREE            (gb_tree_get_type())
#define GB_TREE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TREE, GbTree))
#define GB_TREE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TREE, GbTree const))
#define GB_TREE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_TREE, GbTreeClass))
#define GB_IS_TREE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_TREE))
#define GB_IS_TREE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_TREE))
#define GB_TREE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_TREE, GbTreeClass))

typedef struct _GbTree        GbTree;
typedef struct _GbTreeClass   GbTreeClass;
typedef struct _GbTreePrivate GbTreePrivate;

struct _GbTree
{
	GtkTreeView parent;

	/*< private >*/
	GbTreePrivate *priv;
};

struct _GbTreeClass
{
	GtkTreeViewClass parent_class;
};

void          gb_tree_add_builder    (GbTree        *tree,
                                      GbTreeBuilder *builder);
GtkUIManager *gb_tree_get_menu_ui    (GbTree        *tree);
GtkTreePath  *gb_tree_get_path       (GbTree        *tree,
                                      GList         *list);
GbTreeNode   *gb_tree_get_selected   (GbTree        *tree);
GType         gb_tree_get_type       (void) G_GNUC_CONST;
void          gb_tree_rebuild        (GbTree        *tree);
void          gb_tree_remove_builder (GbTree        *tree,
                                      GbTreeBuilder *builder);
void          gb_tree_append         (GbTree        *tree,
                                      GbTreeNode    *node,
                                      GbTreeNode    *child);
void          gb_tree_prepend        (GbTree        *tree,
                                      GbTreeNode    *node,
                                      GbTreeNode    *child);
GbTree       *gb_tree_node_get_tree  (GbTreeNode    *node);

G_END_DECLS

#endif /* GB_TREE_H */
