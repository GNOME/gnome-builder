/* ide-source-snippet-completion-item.h
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

#include <gtksourceview/gtksource.h>

#include "snippets/ide-source-snippet.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_COMPLETION_ITEM (ide_source_snippet_completion_item_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippetCompletionItem, ide_source_snippet_completion_item,
                      IDE, SOURCE_SNIPPET_COMPLETION_ITEM, GObject)

GtkSourceCompletionProposal *ide_source_snippet_completion_item_new         (IdeSourceSnippet *snippet);
IdeSourceSnippet             *ide_source_snippet_completion_item_get_snippet (IdeSourceSnippetCompletionItem *item);
void                         ide_source_snippet_completion_item_set_snippet (IdeSourceSnippetCompletionItem *item,
                                                                            IdeSourceSnippet               *snippet);

G_END_DECLS
