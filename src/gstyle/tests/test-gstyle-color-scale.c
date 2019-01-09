/* test-gstyle-color-scale.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

  window = g_object_new (GTK_TYPE_WINDOW,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "default-width", 400,
                         "default-height", 100,
                         "visible", TRUE,
                         NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "expand", TRUE,
                      "spacing", 1,
                      "visible", TRUE,
                      NULL);

  gtk_container_add (GTK_CONTAINER (window), box);

  adj = gtk_adjustment_new (0.0, 0.0, 360.0, 0.1, 1.0, 0.0);
  color_scale = g_object_new (GSTYLE_TYPE_COLOR_SCALE,
                              "adjustment", adj,
                              "draw-value", FALSE,
                              "expand", TRUE,
                              "valign", GTK_ALIGN_CENTER,
                              "halign", GTK_ALIGN_FILL,
                              "visible", TRUE,
                              NULL);

  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (color_scale));
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
