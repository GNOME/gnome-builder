/* ide-source-snippet.h
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

#include <gtk/gtk.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET (ide_source_snippet_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippet, ide_source_snippet, IDE, SOURCE_SNIPPET, GObject)

IdeSourceSnippet        *ide_source_snippet_new              (const gchar           *trigger,
                                                              const gchar           *language);
IdeSourceSnippet        *ide_source_snippet_copy             (IdeSourceSnippet      *self);
const gchar             *ide_source_snippet_get_trigger      (IdeSourceSnippet      *self);
void                     ide_source_snippet_set_trigger      (IdeSourceSnippet      *self,
                                                              const gchar           *trigger);
const gchar             *ide_source_snippet_get_language     (IdeSourceSnippet      *self);
void                     ide_source_snippet_set_language     (IdeSourceSnippet      *self,
                                                              const gchar           *language);
const gchar             *ide_source_snippet_get_description  (IdeSourceSnippet      *self);
void                     ide_source_snippet_set_description  (IdeSourceSnippet      *self,
                                                              const gchar           *description);
void                     ide_source_snippet_add_chunk        (IdeSourceSnippet      *self,
                                                              IdeSourceSnippetChunk *chunk);
guint                    ide_source_snippet_get_n_chunks     (IdeSourceSnippet      *self);
gint                     ide_source_snippet_get_tab_stop     (IdeSourceSnippet      *self);
IdeSourceSnippetChunk   *ide_source_snippet_get_nth_chunk    (IdeSourceSnippet      *self,
                                                              guint                  n);
void                     ide_source_snippet_get_chunk_range  (IdeSourceSnippet      *self,
                                                              IdeSourceSnippetChunk *chunk,
                                                              GtkTextIter           *begin,
                                                              GtkTextIter           *end);
IdeSourceSnippetContext *ide_source_snippet_get_context      (IdeSourceSnippet      *self);
const gchar             *ide_source_snippet_get_snippet_text (IdeSourceSnippet      *self);
void                     ide_source_snippet_set_snippet_text (IdeSourceSnippet      *self,
                                                              const gchar           *snippet_text);

G_END_DECLS
