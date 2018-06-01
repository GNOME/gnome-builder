/* ide-snippet-context.h
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

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_CONTEXT (ide_snippet_context_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSnippetContext, ide_snippet_context,
                      IDE, SNIPPET_CONTEXT, GObject)

IDE_AVAILABLE_IN_ALL
IdeSnippetContext *ide_snippet_context_new                 (void);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_emit_changed        (IdeSnippetContext *context);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_clear_variables     (IdeSnippetContext *context);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_add_variable        (IdeSnippetContext *context,
                                                                         const gchar             *key,
                                                                         const gchar             *value);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_add_shared_variable (IdeSnippetContext *context,
                                                                         const gchar             *key,
                                                                         const gchar             *value);
IDE_AVAILABLE_IN_ALL
const gchar             *ide_snippet_context_get_variable        (IdeSnippetContext *context,
                                                                         const gchar             *key);
IDE_AVAILABLE_IN_ALL
gchar                   *ide_snippet_context_expand              (IdeSnippetContext *context,
                                                                         const gchar             *input);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_set_tab_width       (IdeSnippetContext *context,
                                                                         gint                     tab_size);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_set_use_spaces      (IdeSnippetContext *context,
                                                                         gboolean                 use_spaces);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_set_line_prefix     (IdeSnippetContext *context,
                                                                         const gchar             *line_prefix);
IDE_AVAILABLE_IN_ALL
void                     ide_snippet_context_dump                (IdeSnippetContext *context);

G_END_DECLS
