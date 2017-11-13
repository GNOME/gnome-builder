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

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET (ide_source_snippet_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippet, ide_source_snippet, IDE, SOURCE_SNIPPET, GObject)

IDE_AVAILABLE_IN_ALL
IdeSourceSnippet        *ide_source_snippet_new              (const gchar           *trigger,
                                                              const gchar           *language);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippet        *ide_source_snippet_copy             (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_source_snippet_get_trigger      (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_set_trigger      (IdeSourceSnippet      *self,
                                                              const gchar           *trigger);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_source_snippet_get_language     (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_set_language     (IdeSourceSnippet      *self,
                                                              const gchar           *language);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_source_snippet_get_description  (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_set_description  (IdeSourceSnippet      *self,
                                                              const gchar           *description);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_add_chunk        (IdeSourceSnippet      *self,
                                                              IdeSourceSnippetChunk *chunk);
IDE_AVAILABLE_IN_ALL
guint                    ide_source_snippet_get_n_chunks     (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
gint                     ide_source_snippet_get_tab_stop     (IdeSourceSnippet      *self);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippetChunk   *ide_source_snippet_get_nth_chunk    (IdeSourceSnippet      *self,
                                                              guint                  n);
IDE_AVAILABLE_IN_ALL
void                     ide_source_snippet_get_chunk_range  (IdeSourceSnippet      *self,
                                                              IdeSourceSnippetChunk *chunk,
                                                              GtkTextIter           *begin,
                                                              GtkTextIter           *end);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippetContext *ide_source_snippet_get_context      (IdeSourceSnippet      *self);

G_END_DECLS
