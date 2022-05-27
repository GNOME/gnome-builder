/* ide-text-iter.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>
#include <libide-core.h>

G_BEGIN_DECLS

/* Semi-public API */

typedef gboolean (* IdeTextIterCharPredicate)    (GtkTextIter              *iter,
                                                  gunichar                  ch,
                                                  gpointer                  user_data);

IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_find_char        (GtkTextIter              *iter,
                                                  IdeTextIterCharPredicate  pred,
                                                  gpointer                  user_data,
                                                  const GtkTextIter        *limit);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_find_char       (GtkTextIter              *iter,
                                                  IdeTextIterCharPredicate  pred,
                                                  gpointer                  user_data,
                                                  const GtkTextIter        *limit);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_word_start       (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_WORD_start       (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_word_end         (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_WORD_end         (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_paragraph_start (GtkTextIter              *iter);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_paragraph_end    (GtkTextIter              *iter);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_sentence_start  (GtkTextIter              *iter);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_forward_sentence_end     (GtkTextIter              *iter);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_WORD_start      (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_word_start      (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_WORD_end        (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_backward_word_end        (GtkTextIter              *iter,
                                                  gboolean                  newline_stop);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_in_string                (GtkTextIter              *iter,
                                                  const gchar              *str,
                                                  GtkTextIter              *str_start,
                                                  GtkTextIter              *str_end,
                                                  gboolean                  include_str_bounds);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_find_chars_backward      (GtkTextIter              *iter,
                                                  GtkTextIter              *limit,
                                                  GtkTextIter              *end,
                                                  const gchar              *str,
                                                  gboolean                  only_at_start);
IDE_AVAILABLE_IN_ALL
gboolean  ide_text_iter_find_chars_forward       (GtkTextIter              *iter,
                                                  GtkTextIter              *limit,
                                                  GtkTextIter              *end,
                                                  const gchar              *str,
                                                  gboolean                  only_at_start);
IDE_AVAILABLE_IN_ALL
gchar    *ide_text_iter_current_symbol           (const GtkTextIter        *iter,
                                                  GtkTextIter              *out_begin);

G_END_DECLS
