/* gb-document-grid.h
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

#ifndef GB_DOCUMENT_GRID_H
#define GB_DOCUMENT_GRID_H

#include <gtk/gtk.h>

#include "gb-document-manager.h"
#include "gb-document-stack.h"

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT_GRID            (gb_document_grid_get_type())
#define GB_DOCUMENT_GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_GRID, GbDocumentGrid))
#define GB_DOCUMENT_GRID_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_GRID, GbDocumentGrid const))
#define GB_DOCUMENT_GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DOCUMENT_GRID, GbDocumentGridClass))
#define GB_IS_DOCUMENT_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DOCUMENT_GRID))
#define GB_IS_DOCUMENT_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DOCUMENT_GRID))
#define GB_DOCUMENT_GRID_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DOCUMENT_GRID, GbDocumentGridClass))

typedef struct _GbDocumentGrid        GbDocumentGrid;
typedef struct _GbDocumentGridClass   GbDocumentGridClass;
typedef struct _GbDocumentGridPrivate GbDocumentGridPrivate;

struct _GbDocumentGrid
{
  GtkBin parent;

  /*< private >*/
  GbDocumentGridPrivate *priv;
};

struct _GbDocumentGridClass
{
  GtkBinClass parent;
};

GType              gb_document_grid_get_type             (void);
GtkWidget         *gb_document_grid_new                  (void);
void               gb_document_grid_set_document_manager (GbDocumentGrid    *grid,
                                                          GbDocumentManager *manager);
GbDocumentManager *gb_document_grid_get_document_manager (GbDocumentGrid    *grid);
GtkWidget         *gb_document_grid_add_stack_after      (GbDocumentGrid    *grid,
                                                          GbDocumentStack   *stack);
GtkWidget         *gb_document_grid_add_stack_before     (GbDocumentGrid    *grid,
                                                          GbDocumentStack   *stack);
GtkWidget         *gb_document_grid_get_stack_after      (GbDocumentGrid    *grid,
                                                          GbDocumentStack   *stack);
GtkWidget         *gb_document_grid_get_stack_before     (GbDocumentGrid    *grid,
                                                          GbDocumentStack   *stack);
GList             *gb_document_grid_get_stacks           (GbDocumentGrid    *grid);
void               gb_document_grid_focus_document       (GbDocumentGrid    *grid,
                                                          GbDocument        *document);

G_END_DECLS

#endif /* GB_DOCUMENT_GRID_H */
