/* ide-snippet-private.h
 *
 * Copyright 2013-2019 Christian Hergert <christian@hergert.me>
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

#include <gtk/gtk.h>

#include "ide-snippet.h"

G_BEGIN_DECLS

gboolean         ide_snippet_begin               (IdeSnippet    *self,
                                                  GtkTextBuffer *buffer,
                                                  GtkTextIter   *iter);
void             ide_snippet_pause               (IdeSnippet    *self);
void             ide_snippet_unpause             (IdeSnippet    *self);
void             ide_snippet_finish              (IdeSnippet    *self);
gboolean         ide_snippet_move_next           (IdeSnippet    *self);
gboolean         ide_snippet_move_previous       (IdeSnippet    *self);
void             ide_snippet_before_insert_text  (IdeSnippet    *self,
                                                  GtkTextBuffer *buffer,
                                                  GtkTextIter   *iter,
                                                  gchar         *text,
                                                  gint           len);
void             ide_snippet_after_insert_text   (IdeSnippet    *self,
                                                  GtkTextBuffer *buffer,
                                                  GtkTextIter   *iter,
                                                  gchar         *text,
                                                  gint           len);
void             ide_snippet_before_delete_range (IdeSnippet    *self,
                                                  GtkTextBuffer *buffer,
                                                  GtkTextIter   *begin,
                                                  GtkTextIter   *end);
void             ide_snippet_after_delete_range  (IdeSnippet    *self,
                                                  GtkTextBuffer *buffer,
                                                  GtkTextIter   *begin,
                                                  GtkTextIter   *end);
gboolean         ide_snippet_insert_set          (IdeSnippet    *self,
                                                  GtkTextMark   *mark);
void             ide_snippet_dump                (IdeSnippet    *self);
GtkTextMark     *ide_snippet_get_mark_begin      (IdeSnippet    *self);
GtkTextMark     *ide_snippet_get_mark_end        (IdeSnippet    *self);

G_END_DECLS
