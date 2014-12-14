/* gb-search-display.h
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

#ifndef GB_SEARCH_DISPLAY_H
#define GB_SEARCH_DISPLAY_H

#include <gtk/gtk.h>

#include "gb-search-types.h"
#include "gb-search-context.h"

G_BEGIN_DECLS

#define GB_TYPE_SEARCH_DISPLAY            (gb_search_display_get_type())
#define GB_SEARCH_DISPLAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY, GbSearchDisplay))
#define GB_SEARCH_DISPLAY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SEARCH_DISPLAY, GbSearchDisplay const))
#define GB_SEARCH_DISPLAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SEARCH_DISPLAY, GbSearchDisplayClass))
#define GB_IS_SEARCH_DISPLAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SEARCH_DISPLAY))
#define GB_IS_SEARCH_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SEARCH_DISPLAY))
#define GB_SEARCH_DISPLAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SEARCH_DISPLAY, GbSearchDisplayClass))

typedef struct _GbSearchDisplay        GbSearchDisplay;
typedef struct _GbSearchDisplayClass   GbSearchDisplayClass;
typedef struct _GbSearchDisplayPrivate GbSearchDisplayPrivate;

struct _GbSearchDisplay
{
  GtkBin parent;

  /*< private >*/
  GbSearchDisplayPrivate *priv;
};

struct _GbSearchDisplayClass
{
  GtkBinClass parent;
};

GType      gb_search_display_get_type    (void);
GtkWidget *gb_search_display_new         (void);
void       gb_search_display_set_context (GbSearchDisplay *display,
                                          GbSearchContext *context);

G_END_DECLS

#endif /* GB_SEARCH_DISPLAY_H */
