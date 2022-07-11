/* test-gstyle-palette-widget.c
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

#include "gstyle-palette-widget.h"

static void
test_palette_widget (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GstylePaletteWidget *palette_widget;
  GstylePalette *palette;
  GFile *file;

  gtk_init (NULL, NULL);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "default-width", 400,
                         "default-height", 900,
                         "visible", TRUE,
                         NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "expand", TRUE,
                      "spacing", 1,
                      "visible", TRUE,
                      NULL);

  palette_widget = g_object_new (GSTYLE_TYPE_PALETTE_WIDGET, NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (palette_widget));

  file = g_file_new_for_path (TEST_DATA_DIR"/palette.gpl");
  palette = gstyle_palette_new_from_file (file, NULL, NULL);
  gstyle_palette_widget_add (palette_widget, palette);
  g_object_unref (file);

  file = g_file_new_for_path (TEST_DATA_DIR"/palette.xml");
  palette = gstyle_palette_new_from_file (file, NULL, NULL);
  gstyle_palette_widget_add (palette_widget, palette);
  g_object_unref (file);

  gstyle_palette_widget_show_palette (palette_widget, palette);

  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/palettewidget", test_palette_widget);

  return g_test_run ();
}
