/* ide-clang-private.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <clang-c/Index.h>
#include <ide.h>

#include "ide-clang-autocleanups.h"
#include "ide-clang-service.h"
#include "ide-clang-symbol-node.h"
#include "ide-clang-translation-unit.h"

G_BEGIN_DECLS

IdeClangTranslationUnit *_ide_clang_translation_unit_new     (IdeContext         *context,
                                                              CXTranslationUnit   tu,
                                                              GFile              *file,
                                                              IdeHighlightIndex  *index,
                                                              gint64              serial);
IdeSymbolNode           *_ide_clang_symbol_node_new          (IdeContext         *context,
                                                              CXCursor            cursor);
CXCursor                 _ide_clang_symbol_node_get_cursor   (IdeClangSymbolNode *self);
GArray                  *_ide_clang_symbol_node_get_children (IdeClangSymbolNode *self);
void                     _ide_clang_symbol_node_set_children (IdeClangSymbolNode *self,
                                                              GArray             *children);

G_END_DECLS
