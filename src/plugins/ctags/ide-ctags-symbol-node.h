/* ide-ctags-symbol-node.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-code.h>

#include "ide-ctags-index.h"
#include "ide-ctags-symbol-resolver.h"

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_SYMBOL_NODE (ide_ctags_symbol_node_get_type())

G_DECLARE_FINAL_TYPE (IdeCtagsSymbolNode, ide_ctags_symbol_node, IDE, CTAGS_SYMBOL_NODE, IdeSymbolNode)

IdeCtagsSymbolNode       *ide_ctags_symbol_node_new            (IdeCtagsSymbolResolver   *resolver,
                                                                IdeCtagsIndex            *index,
                                                                const IdeCtagsIndexEntry *entry);
void                      ide_ctags_symbol_node_take_child     (IdeCtagsSymbolNode       *self,
                                                                IdeCtagsSymbolNode       *child);
guint                     ide_ctags_symbol_node_get_n_children (IdeCtagsSymbolNode       *self);
IdeSymbolNode            *ide_ctags_symbol_node_get_nth_child  (IdeCtagsSymbolNode       *self,
                                                                guint                     nth_child);
const IdeCtagsIndexEntry *ide_ctags_symbol_node_get_entry      (IdeCtagsSymbolNode       *self);

G_END_DECLS
