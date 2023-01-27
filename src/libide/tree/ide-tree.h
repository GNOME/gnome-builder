/* ide-tree.h
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_TREE_INSIDE) && !defined (IDE_TREE_COMPILATION)
# error "Only <libide-tree.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

#include "ide-tree-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_TREE (ide_tree_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTree, ide_tree, IDE, TREE, GtkWidget)

struct _IdeTreeClass
{
  GtkWidgetClass parent_class;
};

IDE_AVAILABLE_IN_ALL
IdeTree     *ide_tree_new                  (void);
IDE_AVAILABLE_IN_ALL
IdeTreeNode *ide_tree_get_root             (IdeTree              *self);
IDE_AVAILABLE_IN_ALL
void         ide_tree_set_root             (IdeTree              *self,
                                            IdeTreeNode          *root);
IDE_AVAILABLE_IN_ALL
GMenuModel  *ide_tree_get_menu_model       (IdeTree              *self);
IDE_AVAILABLE_IN_ALL
void         ide_tree_set_menu_model       (IdeTree              *self,
                                            GMenuModel           *menu_model);
IDE_AVAILABLE_IN_ALL
void         ide_tree_show_popover_at_node (IdeTree              *self,
                                            IdeTreeNode          *node,
                                            GtkPopover           *popover);
IDE_AVAILABLE_IN_ALL
gboolean     ide_tree_is_node_expanded     (IdeTree              *self,
                                            IdeTreeNode          *node);
IDE_AVAILABLE_IN_ALL
void         ide_tree_collapse_node        (IdeTree              *self,
                                            IdeTreeNode          *node);
IDE_AVAILABLE_IN_ALL
void         ide_tree_expand_to_node       (IdeTree              *self,
                                            IdeTreeNode          *node);
IDE_AVAILABLE_IN_ALL
void         ide_tree_expand_node          (IdeTree              *self,
                                            IdeTreeNode          *node);
IDE_AVAILABLE_IN_ALL
void         ide_tree_expand_node_async    (IdeTree              *self,
                                            IdeTreeNode          *node,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean     ide_tree_expand_node_finish   (IdeTree              *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_ALL
IdeTreeNode *ide_tree_get_selected_node    (IdeTree              *self);
IDE_AVAILABLE_IN_ALL
void         ide_tree_set_selected_node    (IdeTree              *self,
                                            IdeTreeNode          *node);
IDE_AVAILABLE_IN_ALL
void         ide_tree_invalidate_all       (IdeTree              *self);

G_END_DECLS
