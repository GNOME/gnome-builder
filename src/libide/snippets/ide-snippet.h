/* ide-snippet.h
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

#include <gtk/gtk.h>

#include "ide-snippet-chunk.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET (ide_snippet_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeSnippet, ide_snippet, IDE, SNIPPET, GObject)

IDE_AVAILABLE_IN_3_30
IdeSnippet              *ide_snippet_new              (const gchar     *trigger,
                                                       const gchar     *language);
IDE_AVAILABLE_IN_3_30
IdeSnippet              *ide_snippet_copy             (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
const gchar             *ide_snippet_get_trigger      (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
void                     ide_snippet_set_trigger      (IdeSnippet      *self,
                                                       const gchar     *trigger);
IDE_AVAILABLE_IN_3_30
const gchar             *ide_snippet_get_language     (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
void                     ide_snippet_set_language     (IdeSnippet      *self,
                                                       const gchar     *language);
IDE_AVAILABLE_IN_3_30
const gchar             *ide_snippet_get_description  (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
void                     ide_snippet_set_description  (IdeSnippet      *self,
                                                       const gchar     *description);
IDE_AVAILABLE_IN_3_30
void                     ide_snippet_add_chunk        (IdeSnippet      *self,
                                                       IdeSnippetChunk *chunk);
IDE_AVAILABLE_IN_3_30
guint                    ide_snippet_get_n_chunks     (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
gint                     ide_snippet_get_tab_stop     (IdeSnippet      *self);
IDE_AVAILABLE_IN_3_30
IdeSnippetChunk         *ide_snippet_get_nth_chunk    (IdeSnippet      *self,
                                                       guint            n);
IDE_AVAILABLE_IN_3_30
void                     ide_snippet_get_chunk_range  (IdeSnippet      *self,
                                                       IdeSnippetChunk *chunk,
                                                       GtkTextIter     *begin,
                                                       GtkTextIter     *end);
IDE_AVAILABLE_IN_3_30
IdeSnippetContext       *ide_snippet_get_context      (IdeSnippet      *self);

G_END_DECLS
