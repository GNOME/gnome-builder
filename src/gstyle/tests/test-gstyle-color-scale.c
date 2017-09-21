/* test-gstyle-color-scale.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gstyle-color-scale.h"

static void
test_color_scale (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GstyleColorScale *color_scale;
  GtkAdjustment *adj;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 400,100);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "expand", TRUE,
                      "spacing", 1,
                      NULL);

  gtk_container_add (GTK_CONTAINER (window), box);

  adj = gtk_adjustment_new (0.0, 0.0, 360.0, 0.1, 1.0, 0.0);
  color_scale = g_object_new (GSTYLE_TYPE_COLOR_SCALE,
                              "adjustment", adj,
                              "draw-value", FALSE,
                              "expand", TRUE,
                              "valign", GTK_ALIGN_CENTER,
                              "halign", GTK_ALIGN_FILL,
                              NULL);

  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (color_scale));

  gtk_widget_show_all (window);
  gtk_main ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Gstyle/colorscale", test_color_scale);

  return g_test_run ();
}
