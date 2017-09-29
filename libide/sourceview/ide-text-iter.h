/* ide-text-iter.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_TEXT_ITER_H
#define IDE_TEXT_ITER_H


#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef gboolean (* IdeTextIterCharPredicate)    (GtkTextIter              *iter,
                                                  gunichar                  ch,
                                                  gpointer                  user_data);

gboolean _ide_text_iter_forward_find_char        (GtkTextIter              *iter,
                                                  IdeTextIterCharPredicate  pred,
                                                  gpointer                  user_data,
                                                  const GtkTextIter        *limit);
gboolean _ide_text_iter_backward_find_char       (GtkTextIter              *iter,
                                                  IdeTextIterCharPredicate  pred,
                                                  gpointer                  user_data,
                                                  const GtkTextIter        *limit);
gboolean _ide_text_iter_forward_word_start       (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_forward_WORD_start       (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_forward_word_end         (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_forward_WORD_end         (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_backward_paragraph_start (GtkTextIter              *iter);
gboolean _ide_text_iter_forward_paragraph_end    (GtkTextIter              *iter);
gboolean _ide_text_iter_backward_sentence_start  (GtkTextIter              *iter);
gboolean _ide_text_iter_forward_sentence_end     (GtkTextIter              *iter);
gboolean _ide_text_iter_backward_WORD_start      (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_backward_word_start      (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_backward_WORD_end        (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_backward_word_end        (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
gboolean _ide_text_iter_in_string                (GtkTextIter              *iter,
                                                  const gchar              *str,
                                                  GtkTextIter              *str_start,
                                                  GtkTextIter              *str_end,
                                                  gboolean                  include_str_bounds);
gboolean _ide_text_iter_find_chars_backward      (GtkTextIter              *iter,
                                                  GtkTextIter              *limit,
                                                  GtkTextIter              *end,
                                                  const gchar              *str,
                                                  gboolean                  only_at_start);
gboolean _ide_text_iter_find_chars_forward       (GtkTextIter              *iter,
                                                  GtkTextIter              *limit,
                                                  GtkTextIter              *end,
                                                  const gchar              *str,
                                                  gboolean                  only_at_start);

G_END_DECLS

#endif /* IDE_TEXT_ITER_H */
