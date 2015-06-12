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
#include <ide.h>

#include "gb-workbench-types.h"

G_BEGIN_DECLS

#define GB_WIDGET_CLASS_TEMPLATE(klass, name) \
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), \
                                              "/org/gnome/builder/ui/"name)
#define GB_WIDGET_CLASS_BIND(klass, TN, field) \
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS (klass), TN, field)
#define GB_WIDGET_CLASS_BIND_PRIVATE(klass, TN, field) \
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), TN, field)

typedef void (*GbWidgetContextHandler) (GtkWidget  *widget,
                                        IdeContext *context);

IdeContext      *gb_widget_get_context         (GtkWidget    *widget);
void             gb_widget_add_style_class     (gpointer      widget,
                                                const gchar  *class_name);
cairo_surface_t *gb_widget_snapshot            (GtkWidget    *widget,
                                                gint          width,
                                                gint          height,
                                                gdouble       alpha,
                                                gboolean      draw_border);
GbWorkbench     *gb_widget_get_workbench       (GtkWidget    *widget);
void             gb_widget_fade_hide           (GtkWidget    *widget);
void             gb_widget_fade_show           (GtkWidget    *widget);
gboolean         gb_widget_activate_action     (GtkWidget    *widget,
                                                const gchar  *prefix,
                                                const gchar  *action_name,
                                                GVariant     *parameter);
void             gb_widget_set_context_handler (gpointer      widget,
                                                GbWidgetContextHandler handler);
gpointer         gb_widget_find_child_typed    (GtkWidget    *widget,
                                                GType         child_type);

G_END_DECLS

#endif /* GB_WIDGET_H */
