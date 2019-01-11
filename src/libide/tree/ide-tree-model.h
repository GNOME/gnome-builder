/* ide-tree-model.h
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

#include "ide-tree.h"
#include "ide-tree-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_TREE_MODEL (ide_tree_model_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeTreeModel, ide_tree_model, IDE, TREE_MODEL, IdeObject)

IDE_AVAILABLE_IN_3_32
IdeTree     *ide_tree_model_get_tree          (IdeTreeModel         *self);
IDE_AVAILABLE_IN_3_32
IdeTreeNode *ide_tree_model_get_root          (IdeTreeModel         *self);
IDE_AVAILABLE_IN_3_32
void         ide_tree_model_set_root          (IdeTreeModel         *self,
                                               IdeTreeNode          *root);
IDE_AVAILABLE_IN_3_32
const gchar *ide_tree_model_get_kind          (IdeTreeModel         *self);
IDE_AVAILABLE_IN_3_32
void         ide_tree_model_set_kind          (IdeTreeModel         *self,
                                               const gchar          *kind);
IDE_AVAILABLE_IN_3_32
IdeTreeNode *ide_tree_model_get_node          (IdeTreeModel         *self,
                                               GtkTreeIter          *iter);
IDE_AVAILABLE_IN_3_32
GtkTreePath *ide_tree_model_get_path_for_node (IdeTreeModel         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
gboolean     ide_tree_model_get_iter_for_node (IdeTreeModel         *self,
                                               GtkTreeIter          *iter,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
void         ide_tree_model_invalidate        (IdeTreeModel         *self,
                                               IdeTreeNode          *node);
IDE_AVAILABLE_IN_3_32
void         ide_tree_model_expand_async      (IdeTreeModel         *self,
                                               IdeTreeNode          *node,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean     ide_tree_model_expand_finish     (IdeTreeModel         *self,
                                               GAsyncResult         *result,
                                               GError              **error);

G_END_DECLS
