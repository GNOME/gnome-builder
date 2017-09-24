/* ide-source-snippet-context.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_CONTEXT (ide_source_snippet_context_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippetContext, ide_source_snippet_context,
                      IDE, SOURCE_SNIPPET_CONTEXT, GObject)

IdeSourceSnippetContext *ide_source_snippet_context_new                 (void);
void                     ide_source_snippet_context_emit_changed        (IdeSourceSnippetContext *context);
void                     ide_source_snippet_context_clear_variables     (IdeSourceSnippetContext *context);
void                     ide_source_snippet_context_add_variable        (IdeSourceSnippetContext *context,
                                                                         const gchar             *key,
                                                                         const gchar             *value);
void                     ide_source_snippet_context_add_shared_variable (IdeSourceSnippetContext *context,
                                                                         const gchar             *key,
                                                                         const gchar             *value);
const gchar             *ide_source_snippet_context_get_variable        (IdeSourceSnippetContext *context,
                                                                         const gchar             *key);
gchar                   *ide_source_snippet_context_expand              (IdeSourceSnippetContext *context,
                                                                         const gchar             *input);
void                     ide_source_snippet_context_set_tab_width       (IdeSourceSnippetContext *context,
                                                                         gint                     tab_size);
void                     ide_source_snippet_context_set_use_spaces      (IdeSourceSnippetContext *context,
                                                                         gboolean                 use_spaces);
void                     ide_source_snippet_context_set_line_prefix     (IdeSourceSnippetContext *context,
                                                                         const gchar             *line_prefix);
void                     ide_source_snippet_context_dump                (IdeSourceSnippetContext *context);

G_END_DECLS
