/* ide-snippet-completion-item.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_COMPLETION_ITEM (ide_snippet_completion_item_get_type())

G_DECLARE_FINAL_TYPE (IdeSnippetCompletionItem, ide_snippet_completion_item, IDE, SNIPPET_COMPLETION_ITEM, GObject)

IdeSnippetCompletionItem *ide_snippet_completion_item_new         (IdeSnippetStorage        *storage,
                                                                   const IdeSnippetInfo     *info);
IdeSnippet               *ide_snippet_completion_item_get_snippet (IdeSnippetCompletionItem *self);
const IdeSnippetInfo     *ide_snippet_completion_item_get_info    (IdeSnippetCompletionItem *self);

G_END_DECLS
