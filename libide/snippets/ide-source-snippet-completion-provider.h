/* ide-source-snippet-completion-provider.h
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

#ifndef IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_H
#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_H

#include <gtksourceview/gtksource.h>

#include "snippets/ide-source-snippets.h"
#include "sourceview/ide-source-view.h"

G_BEGIN_DECLS

#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_PRIORITY 1000
#define IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER     (ide_source_snippet_completion_provider_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippetCompletionProvider, ide_source_snippet_completion_provider, IDE, SOURCE_SNIPPET_COMPLETION_PROVIDER, GObject)

GtkSourceCompletionProvider *ide_source_snippet_completion_provider_new (IdeSourceView     *source_view,
                                                                         IdeSourceSnippets *snippets);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_H */
