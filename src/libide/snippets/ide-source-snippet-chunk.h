/* ide-source-snippet-chunk.h
 *
 * Copyright 2013 Christian Hergert <christian@hergert.me>
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

#include "ide-types.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_CHUNK (ide_source_snippet_chunk_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSourceSnippetChunk, ide_source_snippet_chunk,
                      IDE, SOURCE_SNIPPET_CHUNK, GObject)

IDE_AVAILABLE_IN_ALL
IdeSourceSnippetChunk   *ide_source_snippet_chunk_new          (void);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippetChunk   *ide_source_snippet_chunk_copy         (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippetContext *ide_source_snippet_chunk_get_context  (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_chunk_set_context  (IdeSourceSnippetChunk   *chunk,
                                                                IdeSourceSnippetContext *context);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_source_snippet_chunk_get_spec     (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_chunk_set_spec     (IdeSourceSnippetChunk   *chunk,
                                                                const gchar             *spec);
IDE_AVAILABLE_IN_ALL
gint                     ide_source_snippet_chunk_get_tab_stop (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_chunk_set_tab_stop (IdeSourceSnippetChunk   *chunk,
                                                                gint                    tab_stop);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_source_snippet_chunk_get_text     (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_chunk_set_text     (IdeSourceSnippetChunk   *chunk,
                                                                const gchar             *text);
IDE_AVAILABLE_IN_ALL
gboolean                 ide_source_snippet_chunk_get_text_set (IdeSourceSnippetChunk   *chunk);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_chunk_set_text_set (IdeSourceSnippetChunk   *chunk,
                                                                gboolean                text_set);

G_END_DECLS
