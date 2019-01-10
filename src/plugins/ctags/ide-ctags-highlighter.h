/* ide-ctags-highlighter.h
 *
 * Copyright 2015 Dimitris Zenios <dimitris.zenios@gmail.com>
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

#include <libide-core.h>

#include "ide-ctags-index.h"

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_HIGHLIGHTER (ide_ctags_highlighter_get_type())

#define IDE_CTAGS_HIGHLIGHTER_TYPE          "def:type"
#define IDE_CTAGS_HIGHLIGHTER_FUNCTION_NAME "def:function"
#define IDE_CTAGS_HIGHLIGHTER_ENUM_NAME     "def:constant"
#define IDE_CTAGS_HIGHLIGHTER_IMPORT        "def:preprocessor"

G_DECLARE_FINAL_TYPE (IdeCtagsHighlighter, ide_ctags_highlighter, IDE, CTAGS_HIGHLIGHTER, IdeObject)

void ide_ctags_highlighter_add_index (IdeCtagsHighlighter *self,
                                      IdeCtagsIndex       *index);

G_END_DECLS
