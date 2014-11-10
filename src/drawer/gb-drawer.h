/* gb-drawer.h
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

#ifndef GB_DRAWER_H
#define GB_DRAWER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_DRAWER            (gb_drawer_get_type())
#define GB_DRAWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DRAWER, GbDrawer))
#define GB_DRAWER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DRAWER, GbDrawer const))
#define GB_DRAWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DRAWER, GbDrawerClass))
#define GB_IS_DRAWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DRAWER))
#define GB_IS_DRAWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DRAWER))
#define GB_DRAWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DRAWER, GbDrawerClass))

typedef struct _GbDrawer        GbDrawer;
typedef struct _GbDrawerClass   GbDrawerClass;
typedef struct _GbDrawerPrivate GbDrawerPrivate;

struct _GbDrawer
{
  GtkBin parent;

  /*< private >*/
  GbDrawerPrivate *priv;
};

struct _GbDrawerClass
{
  GtkBinClass parent;
};

GType      gb_drawer_get_type         (void);
GtkWidget *gb_drawer_new              (void);
GtkWidget *gb_drawer_get_current_page (GbDrawer  *drawer);
void       gb_drawer_set_current_page (GbDrawer  *drawer,
                                       GtkWidget *widget);

G_END_DECLS

#endif /* GB_DRAWER_H */
