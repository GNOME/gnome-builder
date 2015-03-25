/* ide-highlight-engine.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_HIGHLIGHT_ENGINE_H
#define IDE_HIGHLIGHT_ENGINE_H

#include "ide-buffer.h"
#include "ide-highlighter.h"

G_BEGIN_DECLS

#define IDE_TYPE_HIGHLIGHT_ENGINE (ide_highlight_engine_get_type())

G_DECLARE_FINAL_TYPE (IdeHighlightEngine, ide_highlight_engine, IDE, HIGHLIGHT_ENGINE, GObject)

IdeHighlightEngine *ide_highlight_engine_new             (IdeBuffer          *buffer);
IdeBuffer          *ide_highlight_engine_get_buffer      (IdeHighlightEngine *self);
IdeHighlighter     *ide_highlight_engine_get_highlighter (IdeHighlightEngine *self);
void                ide_highlight_engine_set_highlighter (IdeHighlightEngine *self,
                                                          IdeHighlighter     *highlighter);

G_END_DECLS

#endif /* IDE_HIGHLIGHT_ENGINE_H */
