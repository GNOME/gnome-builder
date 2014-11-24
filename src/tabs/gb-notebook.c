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

#define G_LOG_DOMAIN "notebook"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-notebook.h"
#include "gb-tab-label.h"
#include "gb-tab-label-private.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbNotebook, gb_notebook, GTK_TYPE_NOTEBOOK)

#if 0
# define HIDE_CLOSE_Button
#endif

GtkWidget *
gb_notebook_new (void)
{
   return g_object_new (GB_TYPE_NOTEBOOK, NULL);
}

void
gb_notebook_raise_tab (GbNotebook *notebook,
                       GbTab      *tab)
{
  gint page = -1;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GB_IS_TAB (tab));

  if (gtk_widget_get_parent (GTK_WIDGET (tab)) != GTK_WIDGET (notebook))
    {
      g_warning (_("Cannot raise tab, does not belong to requested notebook."));
      return;
    }

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &page,
                           NULL);

  if (page != -1)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
      gtk_widget_grab_focus (GTK_WIDGET (tab));
    }
}

static void
gb_notebook_tab_label_close_clicked (GbNotebook *notebook,
                                     GbTabLabel *tab_label)
{
  GbTab *tab;

  ENTRY;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GB_IS_TAB_LABEL (tab_label));

  tab = gb_tab_label_get_tab (tab_label);
  if (tab)
    gb_tab_close (tab);

  EXIT;
}

void
gb_notebook_add_tab (GbNotebook *notebook,
                     GbTab      *tab)
{
   GbTabLabel *tab_label;

   g_return_if_fail (GB_IS_NOTEBOOK (notebook));
   g_return_if_fail (GB_IS_TAB (tab));

   tab_label = g_object_new (GB_TYPE_TAB_LABEL,
                             "tab", tab,
                             "visible", TRUE,
                             NULL);
   g_signal_connect_object (tab_label,
                            "close-clicked",
                            G_CALLBACK (gb_notebook_tab_label_close_clicked),
                            notebook,
                            G_CONNECT_SWAPPED);

   gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                             GTK_WIDGET (tab),
                             GTK_WIDGET (tab_label));

   gtk_container_child_set (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                            "detachable", TRUE,
                            "reorderable", TRUE,
                            "tab-expand", TRUE,
                            "tab-fill", TRUE,
                            NULL);
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
gb_notebook_switch_page (GtkNotebook *notebook,
                         GtkWidget   *page,
                         guint        page_num)
{
#ifdef HIDE_CLOSE_Button
  GtkWidget *tab_label;
  GtkWidget *prev_page;
  gint prev_page_num;
#endif

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));

#ifdef HIDE_CLOSE_Button
  prev_page_num = gtk_notebook_get_current_page (notebook);

  if (prev_page_num != -1)
    {
      prev_page = gtk_notebook_get_nth_page (notebook, prev_page_num);
      tab_label = gtk_notebook_get_tab_label (notebook, prev_page);
      _gb_tab_label_set_show_close_button (GB_TAB_LABEL (tab_label), FALSE);
    }
#endif

  GTK_NOTEBOOK_CLASS (gb_notebook_parent_class)->switch_page (notebook,
                                                              page,
                                                              page_num);

#ifdef HIDE_CLOSE_Button
  tab_label = gtk_notebook_get_tab_label (notebook, page);
  _gb_tab_label_set_show_close_button (GB_TAB_LABEL (tab_label), TRUE);
#endif
}

static void
gb_notebook_class_init (GbNotebookClass *klass)
{
   GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
   GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

   widget_class->drag_begin = gb_notebook_drag_begin;

   notebook_class->switch_page = gb_notebook_switch_page;
}

static void
gb_notebook_init (GbNotebook *notebook)
{
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
}
