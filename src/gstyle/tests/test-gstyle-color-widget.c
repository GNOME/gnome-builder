/* test-gstyle-color-widget.c
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

#include "gstyle-color-widget.h"

static GstyleColorWidget *
create_color_swatch (const gchar *color_str)
{
  GstyleColor *color;
  GstyleColorWidget *swatch;

  color = gstyle_color_new_from_string ("test", color_str);

  swatch = g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                         "halign", GTK_ALIGN_FILL,
                         "color", color,
                         "name-visible", FALSE,
                         "fallback-name-visible", FALSE,
                         "visible", TRUE,
                         NULL);

  return swatch;
}

static void
test_color_widget (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GstyleColorWidget *swatch;

  gtk_init (NULL, NULL);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "default-width", 200,
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

  swatch = create_color_swatch ("#5080FF");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (swatch));

  swatch = create_color_swatch ("#8010A0");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (swatch));

  swatch = create_color_swatch ("rgba(0, 100, 200, 0.5)");
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (swatch));

  gtk_main ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Gstyle/colorwidget", test_color_widget);

  return g_test_run ();
}
