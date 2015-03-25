/* ide-highlighter.h
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

#ifndef IDE_HIGHLIGHTER_H
#define IDE_HIGHLIGHTER_H

#include <gtk/gtk.h>

#include "ide-buffer.h"
#include "ide-object.h"
#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_HIGHLIGHTER (ide_highlighter_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeHighlighter, ide_highlighter, IDE, HIGHLIGHTER, IdeObject)

typedef enum
{
  IDE_HIGHLIGHT_KIND_NONE,

  IDE_HIGHLIGHT_KIND_TYPE_NAME,
  IDE_HIGHLIGHT_KIND_CLASS_NAME,
  IDE_HIGHLIGHT_KIND_FUNCTION_NAME,
  IDE_HIGHLIGHT_KIND_MACRO_NAME,

  IDE_HIGHLIGHT_KIND_LAST
} IdeHighlightKind;

struct _IdeHighlighterClass
{
  IdeObjectClass parent;

  IdeHighlightKind (*next) (IdeHighlighter    *self,
                            const GtkTextIter *range_begin,
                            const GtkTextIter *range_end,
                            GtkTextIter       *match_begin,
                            GtkTextIter       *match_end);
};

IdeHighlightKind ide_highlighter_next (IdeHighlighter    *self,
                                       const GtkTextIter *range_begin,
                                       const GtkTextIter *range_end,
                                       GtkTextIter       *match_begin,
                                       GtkTextIter       *match_end);

G_END_DECLS

#endif /* IDE_HIGHLIGHTER_H */
