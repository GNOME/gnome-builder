/* gb-search-box.h
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

#ifndef GB_SEARCH_BOX_H
#define GB_SEARCH_BOX_H

#include <gtk/gtk.h>

#include "gb-search-manager.h"

G_BEGIN_DECLS

#define GB_TYPE_SEARCH_BOX            (gb_search_box_get_type())
#define GB_SEARCH_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_BOX, GbSearchBox))
#define GB_SEARCH_BOX_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_BOX, GbSearchBox const))
#define GB_SEARCH_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_BOX, GbSearchBoxClass))
#define GB_IS_SEARCH_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_BOX))
#define GB_IS_SEARCH_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_BOX))
#define GB_SEARCH_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_BOX, GbSearchBoxClass))

typedef struct _GbSearchBox        GbSearchBox;
typedef struct _GbSearchBoxClass   GbSearchBoxClass;
typedef struct _GbSearchBoxPrivate GbSearchBoxPrivate;

struct _GbSearchBox
{
  GtkBox parent;

  /*< private >*/
  GbSearchBoxPrivate *priv;
};

struct _GbSearchBoxClass
{
  GtkBoxClass parent;
};

GType            gb_search_box_get_type           (void);
GtkWidget       *gb_search_box_new                (void);
GbSearchManager *gb_search_box_get_search_manager (GbSearchBox     *box);
void             gb_search_box_set_search_manager (GbSearchBox     *box,
                                                   GbSearchManager *search_manager);

G_END_DECLS

#endif /* GB_SEARCH_BOX_H */
