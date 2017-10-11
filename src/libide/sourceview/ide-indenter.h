/* ide-indenter.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <gtk/gtk.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_INDENTER (ide_indenter_get_type())

G_DECLARE_INTERFACE (IdeIndenter, ide_indenter, IDE, INDENTER, IdeObject)

struct _IdeIndenterInterface
{
  GTypeInterface parent;

  gchar    *(*format)      (IdeIndenter   *self,
                            GtkTextView   *text_view,
                            GtkTextIter   *begin,
                            GtkTextIter   *end,
                            gint          *cursor_offset,
                            GdkEventKey   *event);
  gboolean  (*is_trigger)  (IdeIndenter   *self,
                            GdkEventKey   *event);
};

gboolean  ide_indenter_is_trigger (IdeIndenter *self,
                                   GdkEventKey *event);
gchar    *ide_indenter_format     (IdeIndenter *self,
                                   GtkTextView *text_view,
                                   GtkTextIter *begin,
                                   GtkTextIter *end,
                                   gint        *cursor_offset,
                                   GdkEventKey *event);

G_END_DECLS
