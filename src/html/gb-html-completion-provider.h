/* gb-html-completion-provider.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_HTML_COMPLETION_PROVIDER_H
#define GB_HTML_COMPLETION_PROVIDER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_HTML_COMPLETION_PROVIDER            (gb_html_completion_provider_get_type())
#define GB_HTML_COMPLETION_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_HTML_COMPLETION_PROVIDER, GbHtmlCompletionProvider))
#define GB_HTML_COMPLETION_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_HTML_COMPLETION_PROVIDER, GbHtmlCompletionProvider const))
#define GB_HTML_COMPLETION_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_HTML_COMPLETION_PROVIDER, GbHtmlCompletionProviderClass))
#define GB_IS_HTML_COMPLETION_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_HTML_COMPLETION_PROVIDER))
#define GB_IS_HTML_COMPLETION_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_HTML_COMPLETION_PROVIDER))
#define GB_HTML_COMPLETION_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_HTML_COMPLETION_PROVIDER, GbHtmlCompletionProviderClass))

typedef struct _GbHtmlCompletionProvider        GbHtmlCompletionProvider;
typedef struct _GbHtmlCompletionProviderClass   GbHtmlCompletionProviderClass;
typedef struct _GbHtmlCompletionProviderPrivate GbHtmlCompletionProviderPrivate;

struct _GbHtmlCompletionProvider
{
  GObject parent;

  /*< private >*/
  GbHtmlCompletionProviderPrivate *priv;
};

struct _GbHtmlCompletionProviderClass
{
  GObjectClass parent;
};

GType                        gb_html_completion_provider_get_type (void);
GtkSourceCompletionProvider *gb_html_completion_provider_new      (void);

G_END_DECLS

#endif /* GB_HTML_COMPLETION_PROVIDER_H */
