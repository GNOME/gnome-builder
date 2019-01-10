/* ide-tree.h
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

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-tree-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_TREE (ide_tree_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_DERIVABLE_TYPE (IdeTree, ide_tree, IDE, TREE, GtkTreeView)

struct _IdeTreeClass
{
  GtkTreeViewClass parent_type;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_3_32
GtkWidget   *ide_tree_new                  (void);
IDE_AVAILABLE_IN_3_32
void         ide_tree_set_context_menu     (IdeTree     *self,
                                            GMenu       *menu);
IDE_AVAILABLE_IN_3_32
void         ide_tree_show_popover_at_node (IdeTree     *self,
                                            IdeTreeNode *node,
                                            GtkPopover  *popover);
IDE_AVAILABLE_IN_3_32
IdeTreeNode *ide_tree_get_selected_node    (IdeTree     *self);
IDE_AVAILABLE_IN_3_32
void         ide_tree_select_node          (IdeTree     *self,
                                            IdeTreeNode *node);
IDE_AVAILABLE_IN_3_32
void         ide_tree_expand_node          (IdeTree     *self,
                                            IdeTreeNode *node);
IDE_AVAILABLE_IN_3_32
void         ide_tree_collapse_node        (IdeTree     *self,
                                            IdeTreeNode *node);
IDE_AVAILABLE_IN_3_32
gboolean     ide_tree_node_expanded        (IdeTree     *self,
                                            IdeTreeNode *node);

G_END_DECLS
