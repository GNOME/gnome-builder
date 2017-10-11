/* ide-source-snippet-private.h
 *
 * Copyright © 2013 Christian Hergert <christian@hergert.me>
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

#include "snippets/ide-source-snippet.h"

G_BEGIN_DECLS

gboolean         ide_source_snippet_begin               (IdeSourceSnippet  *snippet,
                                                         GtkTextBuffer     *buffer,
                                                         GtkTextIter       *iter)    G_GNUC_INTERNAL;
void             ide_source_snippet_pause               (IdeSourceSnippet  *snippet) G_GNUC_INTERNAL;
void             ide_source_snippet_unpause             (IdeSourceSnippet  *snippet) G_GNUC_INTERNAL;
void             ide_source_snippet_finish              (IdeSourceSnippet  *snippet) G_GNUC_INTERNAL;
gboolean         ide_source_snippet_move_next           (IdeSourceSnippet  *snippet);
gboolean         ide_source_snippet_move_previous       (IdeSourceSnippet  *snippet);
void             ide_source_snippet_before_insert_text  (IdeSourceSnippet  *snippet,
                                                         GtkTextBuffer     *buffer,
                                                         GtkTextIter       *iter,
                                                         gchar             *text,
                                                         gint               len)     G_GNUC_INTERNAL;
void             ide_source_snippet_after_insert_text   (IdeSourceSnippet  *snippet,
                                                         GtkTextBuffer     *buffer,
                                                         GtkTextIter       *iter,
                                                         gchar             *text,
                                                         gint               len)     G_GNUC_INTERNAL;
void             ide_source_snippet_before_delete_range (IdeSourceSnippet  *snippet,
                                                         GtkTextBuffer     *buffer,
                                                         GtkTextIter       *begin,
                                                         GtkTextIter       *end)     G_GNUC_INTERNAL;
void             ide_source_snippet_after_delete_range  (IdeSourceSnippet  *snippet,
                                                         GtkTextBuffer     *buffer,
                                                         GtkTextIter       *begin,
                                                         GtkTextIter       *end)     G_GNUC_INTERNAL;
gboolean         ide_source_snippet_insert_set          (IdeSourceSnippet  *snippet,
                                                         GtkTextMark       *mark)    G_GNUC_INTERNAL;
void             ide_source_snippet_dump                (IdeSourceSnippet  *self);


G_END_DECLS
