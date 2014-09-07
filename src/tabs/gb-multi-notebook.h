/* gb-multi-notebook.h
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

#ifndef GB_MULTI_NOTEBOOK_H
#define GB_MULTI_NOTEBOOK_H

#include <gtk/gtk.h>

#include "gb-notebook.h"
#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_MULTI_NOTEBOOK            (gb_multi_notebook_get_type())
#define GB_MULTI_NOTEBOOK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MULTI_NOTEBOOK, GbMultiNotebook))
#define GB_MULTI_NOTEBOOK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MULTI_NOTEBOOK, GbMultiNotebook const))
#define GB_MULTI_NOTEBOOK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_MULTI_NOTEBOOK, GbMultiNotebookClass))
#define GB_IS_MULTI_NOTEBOOK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_MULTI_NOTEBOOK))
#define GB_IS_MULTI_NOTEBOOK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_MULTI_NOTEBOOK))
#define GB_MULTI_NOTEBOOK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_MULTI_NOTEBOOK, GbMultiNotebookClass))

typedef struct _GbMultiNotebook        GbMultiNotebook;
typedef struct _GbMultiNotebookClass   GbMultiNotebookClass;
typedef struct _GbMultiNotebookPrivate GbMultiNotebookPrivate;

struct _GbMultiNotebook
{
  GtkGrid parent;

  /*< private >*/
  GbMultiNotebookPrivate *priv;
};

struct _GbMultiNotebookClass
{
  GtkGridClass parent_class;

  GbNotebook *(*create_window) (GbMultiNotebook *mnb,
                                GbNotebook      *notebook,
                                GtkWidget       *widget,
                                gint             x,
                                gint             y);
};

GType       gb_multi_notebook_get_type            (void) G_GNUC_CONST;
GtkWidget  *gb_multi_notebook_new                 (void);
GbNotebook *gb_multi_notebook_get_active_notebook (GbMultiNotebook *self);
GbTab      *gb_multi_notebook_get_active_tab      (GbMultiNotebook *self);
void        gb_multi_notebook_insert_notebook     (GbMultiNotebook *self,
                                                   GbNotebook      *notebook,
                                                   guint            position);
GList      *gb_multi_notebook_get_all_tabs        (GbMultiNotebook *self);
guint       gb_multi_notebook_get_n_notebooks     (GbMultiNotebook *self);
gboolean    gb_multi_notebook_get_show_tabs       (GbMultiNotebook *self);
void        gb_multi_notebook_set_show_tabs       (GbMultiNotebook *self,
                                                   gboolean         show_tabs);

G_END_DECLS

#endif /* GB_MULTI_NOTEBOOK_H */
