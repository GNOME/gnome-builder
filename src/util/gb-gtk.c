/* gb-gtk.c
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

#include "gb-gtk.h"

struct ScrollState
{
  GtkTextView *view;
  guint        line;
  guint        line_offset;
  gboolean     within_margin;
  gboolean     use_align;
  gdouble      xalign;
  gdouble      yalign;
};

void
gb_gtk_text_buffer_get_iter_at_line_and_offset (GtkTextBuffer *buffer,
                                                GtkTextIter   *iter,
                                                guint          line,
                                                guint          line_offset)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter);

  gtk_text_buffer_get_iter_at_line (buffer, iter, line);

  if (gtk_text_iter_get_line (iter) == line)
    {
      for (; line_offset; line_offset--)
        {
          if (gtk_text_iter_ends_line (iter))
            break;
          if (!gtk_text_iter_forward_char (iter))
            {
              gtk_text_buffer_get_end_iter (buffer, iter);
              break;
            }
        }
    }
}

static gboolean
gb_gtk_text_view_scroll_to_iter_cb (gpointer data)
{
  struct ScrollState *state = data;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  g_assert (state);
  g_assert (GTK_IS_TEXT_VIEW (state->view));

  buffer = gtk_text_view_get_buffer (state->view);

  gb_gtk_text_buffer_get_iter_at_line_and_offset (buffer, &iter, state->line,
                                                  state->line_offset);

  gb_gtk_text_view_scroll_to_iter (state->view, &iter, state->within_margin,
                                   state->use_align, state->xalign,
                                   state->yalign);

  g_object_unref (state->view);
  g_free (state);

  return G_SOURCE_REMOVE;
}

/**
 * gb_gtk_text_view_scroll_to_iter:
 *
 * This function is a wrapper function for gb_gtk_text_view_scroll_to_iter()
 * that will check to see if the text_view has calculated enough of it's
 * internal sizing to be able to scroll to the given iter.
 *
 * If not, an idle timeout is added that will check again until we can
 * reach the target location.
 */
void
gb_gtk_text_view_scroll_to_iter (GtkTextView *text_view,
                                 GtkTextIter *iter,
                                 gdouble      within_margin,
                                 gboolean     use_align,
                                 gdouble      xalign,
                                 gdouble      yalign)
{
  GdkRectangle rect;
  GtkTextIter y_iter;
  struct ScrollState *state;
  gint line_top;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter);

  gtk_text_view_get_iter_location (text_view, iter, &rect);

  gtk_text_view_get_line_at_y (text_view, &y_iter, rect.y + (rect.height / 2),
                               &line_top);

  if (gtk_text_iter_get_line (&y_iter) == gtk_text_iter_get_line (iter))
    {
      gtk_text_view_scroll_to_iter (text_view, iter, within_margin, use_align,
                                    xalign, yalign);
      return;
    }

  state = g_new0 (struct ScrollState, 1);
  state->view = g_object_ref (text_view);
  state->line = gtk_text_iter_get_line (iter);
  state->line_offset = gtk_text_iter_get_line_offset (iter);
  state->within_margin = within_margin;
  state->use_align = use_align;
  state->xalign = xalign;
  state->yalign = yalign;

  g_timeout_add (50, gb_gtk_text_view_scroll_to_iter_cb, state);
}
