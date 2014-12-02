/* gb-gtk.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_GTK_H
#define GB_GTK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void gb_gtk_text_buffer_get_iter_at_line_and_offset (GtkTextBuffer *buffer,
                                                     GtkTextIter   *iter,
                                                     guint          line,
                                                     guint          line_offset);

void gb_gtk_text_view_scroll_to_iter (GtkTextView *text_view,
                                      GtkTextIter *iter,
                                      gdouble      within_margin,
                                      gboolean     use_align,
                                      gdouble      xalign,
                                      gdouble      yalign);

G_END_DECLS

#endif /* GB_GTK_H */
