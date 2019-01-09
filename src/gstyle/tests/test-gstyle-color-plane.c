/* test-gstyle-color-plane.c
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

#include "gstyle-color-plane.h"

static void
mode_changed (GstyleColorPlane *self,
              GtkComboBox      *mode_box)
{
  GtkTreeIter iter;
  GtkTreeModel *mode_store;
  GstyleColorPlaneMode mode;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (GTK_IS_COMBO_BOX (mode_box));

  mode_store = gtk_combo_box_get_model (mode_box);
  if (gtk_combo_box_get_active_iter (mode_box, &iter))
    {
      gtk_tree_model_get (mode_store, &iter, 0, &mode, -1);
      gstyle_color_plane_set_mode (self, mode);
    }
}

static void
test_color_plane (void)
{
  g_autoptr (GtkBuilder) builder = NULL;
  GtkWidget *window;
  GtkWidget *mode_box;
  GtkListStore *mode_store;
  GtkWidget *hue_scale;
  GtkWidget *saturation_scale;
  GtkWidget *value_scale;
  GtkWidget *cielab_l_scale;
  GtkWidget *cielab_a_scale;
  GtkWidget *cielab_b_scale;
  GtkWidget *red_scale;
  GtkWidget *green_scale;
  GtkWidget *blue_scale;
  GtkWidget *box;
  GstyleColorPlane *plane;
  GError *error = NULL;

  gtk_init (NULL, NULL);
  builder = gtk_builder_new ();

  gtk_builder_add_from_file (builder, TEST_DATA_DIR"/gstyle-color-editor.ui", &error);
  g_assert_no_error (error);

  plane = GSTYLE_COLOR_PLANE (gtk_builder_get_object (builder, "plane"));

  hue_scale = GTK_WIDGET (gtk_builder_get_object (builder, "hsv_h_scale"));
  gtk_range_set_adjustment (GTK_RANGE (hue_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_HSV_H));

  saturation_scale = GTK_WIDGET (gtk_builder_get_object (builder, "hsv_s_scale"));
  gtk_range_set_adjustment (GTK_RANGE (saturation_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_HSV_S));

  value_scale = GTK_WIDGET (gtk_builder_get_object (builder, "hsv_v_scale"));
  gtk_range_set_adjustment (GTK_RANGE (value_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_HSV_V));

  cielab_l_scale = GTK_WIDGET (gtk_builder_get_object (builder, "cielab_l_scale"));
  gtk_range_set_adjustment (GTK_RANGE (cielab_l_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_LAB_L));

  cielab_a_scale = GTK_WIDGET (gtk_builder_get_object (builder, "cielab_a_scale"));
  gtk_range_set_adjustment (GTK_RANGE (cielab_a_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_LAB_A));

  cielab_b_scale = GTK_WIDGET (gtk_builder_get_object (builder, "cielab_b_scale"));
  gtk_range_set_adjustment (GTK_RANGE (cielab_b_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_LAB_A));

  red_scale = GTK_WIDGET (gtk_builder_get_object (builder, "rgb_red_scale"));
  gtk_range_set_adjustment (GTK_RANGE (red_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_RGB_RED));

  green_scale = GTK_WIDGET (gtk_builder_get_object (builder, "rgb_green_scale"));
  gtk_range_set_adjustment (GTK_RANGE (green_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_RGB_RED));

  blue_scale = GTK_WIDGET (gtk_builder_get_object (builder, "rgb_blue_scale"));
  gtk_range_set_adjustment (GTK_RANGE (blue_scale),
                            gstyle_color_plane_get_component_adjustment (plane, GSTYLE_COLOR_COMPONENT_RGB_BLUE));

  mode_box = GTK_WIDGET (gtk_builder_get_object (builder, "mode_box"));
  mode_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "mode_store"));
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_HUE, 1, "Hsv Hue", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_SATURATION, 1, "Hsv Saturation", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS, 1, "Hsv Brightness (Value)", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_CIELAB_L, 1, "CieLab L*", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_CIELAB_A, 1, "CieLab a*", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_CIELAB_B, 1, "CieLab b*", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_RED, 1, "rgb red", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_GREEN, 1, "rgb green", -1);
  gtk_list_store_insert_with_values (mode_store, NULL, -1, 0, GSTYLE_COLOR_PLANE_MODE_BLUE, 1, "rgb blue", -1);

  gtk_combo_box_set_active (GTK_COMBO_BOX (mode_box), 0);
  g_signal_connect_swapped (mode_box, "changed", G_CALLBACK (mode_changed), plane);

  box = GTK_WIDGET (gtk_builder_get_object (builder, "editor_box"));
  window = g_object_new (GTK_TYPE_WINDOW,
                         "type", GTK_WINDOW_TOPLEVEL,
                         "default-width", 400,
                         "default-height", 400,
                         "visible", TRUE,
                         NULL);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/colorplane", test_color_plane);

  return g_test_run ();
}
