/* gb-widget.h
 *
 * Copyright (C) 2014 Christian Hergert <christian.hergert@mongodb.com>
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

#ifndef GB_WIDGET_H
#define GB_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void             gb_widget_add_style_class (gpointer      widget,
                                            const gchar  *class_name);
cairo_surface_t *gb_widget_snapshot        (GtkWidget    *widget,
                                            gint          width,
                                            gint          height,
                                            gdouble       alpha,
                                            gboolean      draw_border);
gpointer         gb_widget_get_workbench   (GtkWidget    *widget);
void             gb_widget_fade_hide       (GtkWidget    *widget);
void             gb_widget_fade_show       (GtkWidget    *widget);

G_END_DECLS

#endif /* GB_WIDGET_H */
