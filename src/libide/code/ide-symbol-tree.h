/* ide-symbol-tree.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL_TREE (ide_symbol_tree_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeSymbolTree, ide_symbol_tree, IDE, SYMBOL_TREE, GObject)

struct _IdeSymbolTreeInterface
{
  GTypeInterface parent;

  guint          (*get_n_children) (IdeSymbolTree *self,
                                    IdeSymbolNode *node);
  IdeSymbolNode *(*get_nth_child)  (IdeSymbolTree *self,
                                    IdeSymbolNode *node,
                                    guint          nth);
};

IDE_AVAILABLE_IN_ALL
guint          ide_symbol_tree_get_n_children (IdeSymbolTree *self,
                                               IdeSymbolNode *node);
IDE_AVAILABLE_IN_ALL
IdeSymbolNode *ide_symbol_tree_get_nth_child  (IdeSymbolTree *self,
                                               IdeSymbolNode *node,
                                               guint          nth);

G_END_DECLS
