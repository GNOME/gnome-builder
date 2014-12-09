/* gb-document-view.h
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

#ifndef GB_DOCUMENT_VIEW_H
#define GB_DOCUMENT_VIEW_H

#include <gtk/gtk.h>

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT_VIEW            (gb_document_view_get_type())
#define GB_DOCUMENT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_VIEW, GbDocumentView))
#define GB_DOCUMENT_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_VIEW, GbDocumentView const))
#define GB_DOCUMENT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DOCUMENT_VIEW, GbDocumentViewClass))
#define GB_IS_DOCUMENT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DOCUMENT_VIEW))
#define GB_IS_DOCUMENT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DOCUMENT_VIEW))
#define GB_DOCUMENT_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DOCUMENT_VIEW, GbDocumentViewClass))

typedef struct _GbDocumentView        GbDocumentView;
typedef struct _GbDocumentViewClass   GbDocumentViewClass;
typedef struct _GbDocumentViewPrivate GbDocumentViewPrivate;

struct _GbDocumentView
{
  GtkBox parent;

  /*< private >*/
  GbDocumentViewPrivate *priv;
};

struct _GbDocumentViewClass
{
  GtkBoxClass parent;

  GbDocument  *(*get_document)    (GbDocumentView *view);
  gboolean     (*get_can_preview) (GbDocumentView *view);
  gboolean     (*close)           (GbDocumentView *view);
  GbDocument  *(*create_preview)  (GbDocumentView *view);
};

GType           gb_document_view_get_type        (void);
GbDocumentView *gb_document_view_new             (void);
GbDocument     *gb_document_view_create_preview  (GbDocumentView *view);
void            gb_document_view_close           (GbDocumentView *view);
GbDocument     *gb_document_view_get_document    (GbDocumentView *view);
GtkWidget      *gb_document_view_get_controls    (GbDocumentView *view);
gboolean        gb_document_view_get_can_preview (GbDocumentView *view);

G_END_DECLS

#endif /* GB_DOCUMENT_VIEW_H */
