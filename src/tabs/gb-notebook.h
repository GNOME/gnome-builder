/* gb-notebook.h
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

#ifndef GB_NOTEBOOK_H
#define GB_NOTEBOOK_H

#include <gtk/gtk.h>

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_NOTEBOOK            (gb_notebook_get_type())
#define GB_NOTEBOOK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NOTEBOOK, GbNotebook))
#define GB_NOTEBOOK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_NOTEBOOK, GbNotebook const))
#define GB_NOTEBOOK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_NOTEBOOK, GbNotebookClass))
#define GB_IS_NOTEBOOK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_NOTEBOOK))
#define GB_IS_NOTEBOOK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_NOTEBOOK))
#define GB_NOTEBOOK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_NOTEBOOK, GbNotebookClass))

typedef struct _GbNotebook        GbNotebook;
typedef struct _GbNotebookClass   GbNotebookClass;
typedef struct _GbNotebookPrivate GbNotebookPrivate;

struct _GbNotebook
{
  GtkNotebookClass parent;

  /*< private >*/
  GbNotebookPrivate *priv;
};

struct _GbNotebookClass
{
  GtkNotebookClass parent_class;
};

GType      gb_notebook_get_type  (void) G_GNUC_CONST;
GtkWidget *gb_notebook_new       (void);
void       gb_notebook_add_tab   (GbNotebook *notebook,
                                  GbTab      *tab);
void       gb_notebook_raise_tab (GbNotebook *notebook,
                                  GbTab      *tab);

G_END_DECLS

#endif /* GB_NOTEBOOK_H */
