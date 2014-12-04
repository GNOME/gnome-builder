/* gb-document.h
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

#ifndef GB_DOCUMENT_H
#define GB_DOCUMENT_H

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT               (gb_document_get_type ())
#define GB_DOCUMENT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT, GbDocument))
#define GB_IS_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DOCUMENT))
#define GB_DOCUMENT_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GB_TYPE_DOCUMENT, GbDocumentInterface))

typedef struct _GbDocument      GbDocument;
typedef struct _GbDocumentInterface GbDocumentInterface;

struct _GbDocumentInterface
{
  GTypeInterface parent;

  gboolean     (*get_can_save) (GbDocument *document);
  const gchar *(*get_title)    (GbDocument *document);
  GbTab       *(*create_tab)   (GbDocument *document);
};

GType        gb_document_get_type     (void) G_GNUC_CONST;
gboolean     gb_document_get_can_save (GbDocument *document);
const gchar *gb_document_get_title    (GbDocument *document);
GbTab       *gb_document_create_tab   (GbDocument *document);

G_END_DECLS

#endif /* GB_DOCUMENT_H */
