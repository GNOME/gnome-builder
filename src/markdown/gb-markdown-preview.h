/* gb-markdown-preview.h
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

#ifndef GB_MARKDOWN_PREVIEW_H
#define GB_MARKDOWN_PREVIEW_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define GB_TYPE_MARKDOWN_PREVIEW            (gb_markdown_preview_get_type())
#define GB_MARKDOWN_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MARKDOWN_PREVIEW, GbMarkdownPreview))
#define GB_MARKDOWN_PREVIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MARKDOWN_PREVIEW, GbMarkdownPreview const))
#define GB_MARKDOWN_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_MARKDOWN_PREVIEW, GbMarkdownPreviewClass))
#define GB_IS_MARKDOWN_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_MARKDOWN_PREVIEW))
#define GB_IS_MARKDOWN_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_MARKDOWN_PREVIEW))
#define GB_MARKDOWN_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_MARKDOWN_PREVIEW, GbMarkdownPreviewClass))

typedef struct _GbMarkdownPreview        GbMarkdownPreview;
typedef struct _GbMarkdownPreviewClass   GbMarkdownPreviewClass;
typedef struct _GbMarkdownPreviewPrivate GbMarkdownPreviewPrivate;

struct _GbMarkdownPreview
{
  WebKitWebView parent;

  /*< private >*/
  GbMarkdownPreviewPrivate *priv;
};

struct _GbMarkdownPreviewClass
{
  WebKitWebViewClass parent;
};

GType          gb_markdown_preview_get_type   (void) G_GNUC_CONST;
GtkWidget     *gb_markdown_preview_new        (void);
GtkTextBuffer *gb_markdown_preview_get_buffer (GbMarkdownPreview *preview);
void           gb_markdown_preview_set_buffer (GbMarkdownPreview *preview,
                                               GtkTextBuffer     *buffer);

G_END_DECLS

#endif /* GB_MARKDOWN_PREVIEW_H */
