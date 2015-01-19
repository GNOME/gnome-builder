/* gb-search-display-row.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SEARCH_DISPLAY_ROW_H
#define GB_SEARCH_DISPLAY_ROW_H

#include <gtk/gtk.h>

#include "gb-search-types.h"

G_BEGIN_DECLS

#define GB_SEARCH_DISPLAY_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY_ROW, GbSearchDisplayRow))
#define GB_SEARCH_DISPLAY_ROW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY_ROW, GbSearchDisplayRow const))
#define GB_SEARCH_DISPLAY_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_DISPLAY_ROW, GbSearchDisplayRowClass))
#define GB_IS_SEARCH_DISPLAY_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_DISPLAY_ROW))
#define GB_IS_SEARCH_DISPLAY_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_DISPLAY_ROW))
#define GB_SEARCH_DISPLAY_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_DISPLAY_ROW, GbSearchDisplayRowClass))

struct _GbSearchDisplayRow
{
  GtkBox parent;

  /*< private >*/
  GbSearchDisplayRowPrivate *priv;
};

struct _GbSearchDisplayRowClass
{
  GtkBoxClass parent;
};

GbSearchResult *gb_search_display_row_get_result (GbSearchDisplayRow *row);
void            gb_search_display_row_set_result (GbSearchDisplayRow *row,
                                                  GbSearchResult     *result);

G_END_DECLS

#endif /* GB_SEARCH_DISPLAY_ROW_H */
