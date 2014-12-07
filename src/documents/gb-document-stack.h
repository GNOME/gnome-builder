/* gb-document-stack.h
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

#ifndef GB_DOCUMENT_STACK_H
#define GB_DOCUMENT_STACK_H

#include <gtk/gtk.h>

#include "gb-document-manager.h"
#include "gb-document-split.h"
#include "gb-document-view.h"

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT_STACK            (gb_document_stack_get_type())
#define GB_DOCUMENT_STACK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_STACK, GbDocumentStack))
#define GB_DOCUMENT_STACK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_STACK, GbDocumentStack const))
#define GB_DOCUMENT_STACK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DOCUMENT_STACK, GbDocumentStackClass))
#define GB_IS_DOCUMENT_STACK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DOCUMENT_STACK))
#define GB_IS_DOCUMENT_STACK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DOCUMENT_STACK))
#define GB_DOCUMENT_STACK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DOCUMENT_STACK, GbDocumentStackClass))

typedef struct _GbDocumentStack        GbDocumentStack;
typedef struct _GbDocumentStackClass   GbDocumentStackClass;
typedef struct _GbDocumentStackPrivate GbDocumentStackPrivate;

struct _GbDocumentStack
{
  GtkBox parent;

  /*< private >*/
  GbDocumentStackPrivate *priv;
};

struct _GbDocumentStackClass
{
  GtkBoxClass parent;

  void (*create_view) (GbDocumentStack *stack,
                       GbDocument      *document,
                       GbDocumentSplit  split);
  void (*empty)       (GbDocumentStack *stack);
};

GType              gb_document_stack_get_type             (void);
GtkWidget         *gb_document_stack_new                  (void);
void               gb_document_stack_focus_document       (GbDocumentStack   *stack,
                                                           GbDocument        *document);
GbDocumentManager *gb_document_stack_get_document_manager (GbDocumentStack   *stack);
void               gb_document_stack_set_document_manager (GbDocumentStack   *stack,
                                                           GbDocumentManager *manager);
GbDocumentView    *gb_document_stack_get_active_view      (GbDocumentStack   *stack);
void               gb_document_stack_set_active_view      (GbDocumentStack   *stack,
                                                           GbDocumentView    *view);
GtkWidget         *gb_document_stack_find_with_document   (GbDocumentStack   *stack,
                                                           GbDocument        *document);
GtkWidget         *gb_document_stack_find_with_type       (GbDocumentStack   *stack,
                                                           GType              type_id);

G_END_DECLS

#endif /* GB_DOCUMENT_STACK_H */
