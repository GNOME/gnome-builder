/* ide-highlight-engine.h
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

#define IDE_TYPE_HIGHLIGHT_ENGINE (ide_highlight_engine_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeHighlightEngine, ide_highlight_engine, IDE, HIGHLIGHT_ENGINE, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeHighlightEngine *ide_highlight_engine_new             (IdeBuffer          *buffer);
IDE_AVAILABLE_IN_ALL
IdeBuffer          *ide_highlight_engine_get_buffer      (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
IdeHighlighter     *ide_highlight_engine_get_highlighter (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_rebuild         (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_clear           (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_invalidate      (IdeHighlightEngine *self,
                                                          const GtkTextIter  *begin,
                                                          const GtkTextIter  *end);
IDE_AVAILABLE_IN_ALL
GtkTextTag         *ide_highlight_engine_get_style       (IdeHighlightEngine *self,
                                                          const gchar        *style_name);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_pause           (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_unpause         (IdeHighlightEngine *self);
IDE_AVAILABLE_IN_ALL
void                ide_highlight_engine_advance         (IdeHighlightEngine *self);

G_END_DECLS
