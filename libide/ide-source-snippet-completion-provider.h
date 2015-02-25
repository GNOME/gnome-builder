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

#include <gtksourceview/gtksourcecompletionprovider.h>

#include "ide-source-snippets.h"
#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER            (ide_source_snippet_completion_provider_get_type())
#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, IdeSourceSnippetCompletionProvider))
#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, IdeSourceSnippetCompletionProvider const))
#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, IdeSourceSnippetCompletionProviderClass))
#define IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER))
#define IDE_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER))
#define IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, IdeSourceSnippetCompletionProviderClass))

typedef struct _IdeSourceSnippetCompletionProvider        IdeSourceSnippetCompletionProvider;
typedef struct _IdeSourceSnippetCompletionProviderClass   IdeSourceSnippetCompletionProviderClass;
typedef struct _IdeSourceSnippetCompletionProviderPrivate IdeSourceSnippetCompletionProviderPrivate;

struct _IdeSourceSnippetCompletionProvider
{
  GObject parent;

  /*< private >*/
  IdeSourceSnippetCompletionProviderPrivate *priv;
};

struct _IdeSourceSnippetCompletionProviderClass
{
  GObjectClass parent_class;
};

GType                        ide_source_snippet_completion_provider_get_type (void);
GtkSourceCompletionProvider *ide_source_snippet_completion_provider_new      (IdeSourceView     *source_view,
                                                                             IdeSourceSnippets *snippets);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPET_COMPLETION_PROVIDER_H */
