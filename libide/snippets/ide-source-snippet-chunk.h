/* ide-source-snippet-chunk.h
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_SOURCE_SNIPPET_CHUNK_H
#define IDE_SOURCE_SNIPPET_CHUNK_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_CHUNK (ide_source_snippet_chunk_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippetChunk, ide_source_snippet_chunk,
                      IDE, SOURCE_SNIPPET_CHUNK, GObject)

IdeSourceSnippetChunk   *ide_source_snippet_chunk_new          (void);
IdeSourceSnippetChunk   *ide_source_snippet_chunk_copy         (IdeSourceSnippetChunk   *chunk);
IdeSourceSnippetContext *ide_source_snippet_chunk_get_context  (IdeSourceSnippetChunk   *chunk);
void                     ide_source_snippet_chunk_set_context  (IdeSourceSnippetChunk   *chunk,
                                                                IdeSourceSnippetContext *context);
const gchar             *ide_source_snippet_chunk_get_spec     (IdeSourceSnippetChunk   *chunk);
void                     ide_source_snippet_chunk_set_spec     (IdeSourceSnippetChunk   *chunk,
                                                                const gchar             *spec);
gint                     ide_source_snippet_chunk_get_tab_stop (IdeSourceSnippetChunk   *chunk);
void                     ide_source_snippet_chunk_set_tab_stop (IdeSourceSnippetChunk   *chunk,
                                                                gint                    tab_stop);
const gchar             *ide_source_snippet_chunk_get_text     (IdeSourceSnippetChunk   *chunk);
void                     ide_source_snippet_chunk_set_text     (IdeSourceSnippetChunk   *chunk,
                                                                const gchar             *text);
gboolean                 ide_source_snippet_chunk_get_text_set (IdeSourceSnippetChunk   *chunk);
void                     ide_source_snippet_chunk_set_text_set (IdeSourceSnippetChunk   *chunk,
                                                                gboolean                text_set);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPET_CHUNK_H */
