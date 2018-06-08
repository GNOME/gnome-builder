/* ide-snippet-chunk.h
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

#include <glib-object.h>

#include "ide-version-macros.h"

#include "snippets/ide-snippet-context.h"

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_CHUNK (ide_snippet_chunk_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeSnippetChunk, ide_snippet_chunk, IDE, SNIPPET_CHUNK, GObject)

IDE_AVAILABLE_IN_3_30
IdeSnippetChunk   *ide_snippet_chunk_new          (void);
IDE_AVAILABLE_IN_3_30
IdeSnippetChunk   *ide_snippet_chunk_copy         (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
IdeSnippetContext *ide_snippet_chunk_get_context  (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_chunk_set_context  (IdeSnippetChunk   *chunk,
                                                   IdeSnippetContext *context);
IDE_AVAILABLE_IN_3_30
const gchar       *ide_snippet_chunk_get_spec     (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_chunk_set_spec     (IdeSnippetChunk   *chunk,
                                                   const gchar       *spec);
IDE_AVAILABLE_IN_3_30
gint               ide_snippet_chunk_get_tab_stop (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_chunk_set_tab_stop (IdeSnippetChunk   *chunk,
                                                   gint              tab_stop);
IDE_AVAILABLE_IN_3_30
const gchar       *ide_snippet_chunk_get_text     (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_chunk_set_text     (IdeSnippetChunk   *chunk,
                                                   const gchar       *text);
IDE_AVAILABLE_IN_3_30
gboolean           ide_snippet_chunk_get_text_set (IdeSnippetChunk   *chunk);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_chunk_set_text_set (IdeSnippetChunk   *chunk,
                                                   gboolean          text_set);

G_END_DECLS
