/* gb-html-document.h
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

#ifndef GB_HTML_DOCUMENT_H
#define GB_HTML_DOCUMENT_H

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_HTML_DOCUMENT (gb_html_document_get_type())

G_DECLARE_FINAL_TYPE (GbHtmlDocument, gb_html_document, GB, HTML_DOCUMENT, GObject)

typedef gchar *(*GbHtmlDocumentTransform) (GbHtmlDocument *document,
                                           const gchar    *content);

GtkTextBuffer *gb_html_document_get_buffer         (GbHtmlDocument          *document);
void           gb_html_document_set_transform_func (GbHtmlDocument          *document,
                                                    GbHtmlDocumentTransform  transform);
gchar         *gb_html_document_get_content        (GbHtmlDocument          *document);
gchar         *gb_html_markdown_transform          (GbHtmlDocument          *document,
                                                    const gchar             *content);

G_END_DECLS

#endif /* GB_HTML_DOCUMENT_H */
