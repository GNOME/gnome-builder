/* gb-source-snippet-completion-provider.h
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

#ifndef GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_H
#define GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_H

#include <gtksourceview/gtksourcecompletionprovider.h>

#include "gb-source-snippets.h"
#include "gb-source-view.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER            (gb_source_snippet_completion_provider_get_type())
#define GB_SOURCE_SNIPPET_COMPLETION_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, GbSourceSnippetCompletionProvider))
#define GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, GbSourceSnippetCompletionProvider const))
#define GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, GbSourceSnippetCompletionProviderClass))
#define GB_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER))
#define GB_IS_SOURCE_SNIPPET_COMPLETION_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER))
#define GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER, GbSourceSnippetCompletionProviderClass))

typedef struct _GbSourceSnippetCompletionProvider        GbSourceSnippetCompletionProvider;
typedef struct _GbSourceSnippetCompletionProviderClass   GbSourceSnippetCompletionProviderClass;
typedef struct _GbSourceSnippetCompletionProviderPrivate GbSourceSnippetCompletionProviderPrivate;

struct _GbSourceSnippetCompletionProvider
{
  GObject parent;

  /*< private >*/
  GbSourceSnippetCompletionProviderPrivate *priv;
};

struct _GbSourceSnippetCompletionProviderClass
{
  GObjectClass parent_class;
};

GType                        gb_source_snippet_completion_provider_get_type (void);
GtkSourceCompletionProvider *gb_source_snippet_completion_provider_new      (GbSourceView     *source_view,
                                                                             GbSourceSnippets *snippets);

G_END_DECLS

#endif /* GB_SOURCE_SNIPPET_COMPLETION_PROVIDER_H */
