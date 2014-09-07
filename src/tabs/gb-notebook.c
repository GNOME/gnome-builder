/* gb-notebook.c
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

#include <glib/gi18n.h>

#include "gb-notebook.h"
#include "gb-tab-label.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbNotebook, gb_notebook, GTK_TYPE_NOTEBOOK)

GtkWidget *
gb_notebook_new (void)
{
   return g_object_new (GB_TYPE_NOTEBOOK, NULL);
}

void
gb_notebook_add_tab (GbNotebook *notebook,
                     GbTab      *tab)
{
   GbTabLabel *tab_label;

   g_return_if_fail (GB_IS_NOTEBOOK (notebook));
   g_return_if_fail (GB_IS_TAB (tab));

   gtk_container_add_with_properties (GTK_CONTAINER (notebook),
                                      GTK_WIDGET (tab),
                                      "detachable", TRUE,
                                      "reorderable", TRUE,
                                      "tab-expand", TRUE,
                                      "tab-fill", TRUE,
                                      NULL);

   tab_label = g_object_new (GB_TYPE_TAB_LABEL,
                             "tab", tab,
                             "visible", TRUE,
                             NULL);
   g_object_set_data (G_OBJECT (tab_label), "GB_TAB", tab);
   gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
                               GTK_WIDGET (tab),
                               GTK_WIDGET (tab_label));
}

static void
gb_notebook_drag_begin (GtkWidget      *widget,
                        GdkDragContext *context)
{
   cairo_surface_t *icon;
   GtkWidget *child;
   gint current_page;

   g_return_if_fail (GB_IS_NOTEBOOK (widget));
   g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

   current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (widget));
   child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (widget), current_page);
   icon = gb_widget_snapshot (child, 300, 300, 0.6, TRUE);

   GTK_WIDGET_CLASS (gb_notebook_parent_class)->drag_begin (widget, context);

   if (icon) {
      gtk_drag_set_icon_surface (context, icon);
      cairo_surface_destroy (icon);
   }
}

static void
gb_notebook_class_init (GbNotebookClass *klass)
{
   GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

   widget_class->drag_begin = gb_notebook_drag_begin;
}

static void
gb_notebook_init (GbNotebook *notebook)
{
}
