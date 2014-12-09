/* gb-html-view.h
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

#ifndef GB_HTML_VIEW_H
#define GB_HTML_VIEW_H

#include "gb-document-view.h"
#include "gb-html-document.h"

G_BEGIN_DECLS

#define GB_TYPE_HTML_VIEW            (gb_html_view_get_type())
#define GB_HTML_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_HTML_VIEW, GbHtmlView))
#define GB_HTML_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_HTML_VIEW, GbHtmlView const))
#define GB_HTML_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_HTML_VIEW, GbHtmlViewClass))
#define GB_IS_HTML_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_HTML_VIEW))
#define GB_IS_HTML_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_HTML_VIEW))
#define GB_HTML_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_HTML_VIEW, GbHtmlViewClass))

typedef struct _GbHtmlView        GbHtmlView;
typedef struct _GbHtmlViewClass   GbHtmlViewClass;
typedef struct _GbHtmlViewPrivate GbHtmlViewPrivate;

struct _GbHtmlView
{
  GbDocumentView parent;

  /*< private >*/
  GbHtmlViewPrivate *priv;
};

struct _GbHtmlViewClass
{
  GbDocumentViewClass parent;
};

GType      gb_html_view_get_type (void);
GtkWidget *gb_html_view_new      (GbHtmlDocument *document);

G_END_DECLS

#endif /* GB_HTML_VIEW_H */
