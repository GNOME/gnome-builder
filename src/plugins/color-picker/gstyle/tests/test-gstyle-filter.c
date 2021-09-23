/* test-gstyle-filter.c
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

/* Photos sources:
 *
 * sample0.jpg: https://pixabay.com/en/color-chalk-india-colorful-color-106692/
 * sample1.jpg: https://pixabay.com/en/color-color-picker-color-wheel-1065389/
 */

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gstyle-color-filter.h"
#include "gstyle-utils.h"

typedef struct _FilterData
{
  GdkPixbuf             *src_pixbuf;
  GdkPixbuf             *dst_pixbuf;
  GtkWidget             *src_img;
  GtkWidget             *dst_img;
  GtkListStore          *store;
  GtkListStore          *sample_store;
  GstyleColorFilterFunc  filter_func;
} FilterData;

static void
filter_pixbuf (GdkPixbuf             *src_pixbuf,
               GdkPixbuf             *dst_pixbuf,
               GstyleColorFilterFunc  filter_func)
{
  gint width;
  gint height;
  gint rowstride;
  guint32 *src_data;
  guint32 *dst_data;
  guint32 *src_p;
  guint32 *dst_p;
  GdkRGBA rgba;
  GdkRGBA filtered_rgba;

  g_assert (gdk_pixbuf_get_colorspace (src_pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_colorspace (dst_pixbuf) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (src_pixbuf) == 8);
  g_assert (gdk_pixbuf_get_bits_per_sample (dst_pixbuf) == 8);
  g_assert (gdk_pixbuf_get_has_alpha (src_pixbuf));
  g_assert (gdk_pixbuf_get_has_alpha (dst_pixbuf));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

  src_data = (guint32 *)gdk_pixbuf_get_pixels (src_pixbuf);
  dst_data = (guint32 *)gdk_pixbuf_get_pixels (dst_pixbuf);

#pragma GCC diagnostic pop

  width = gdk_pixbuf_get_width (src_pixbuf);
  height = gdk_pixbuf_get_height (src_pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (src_pixbuf);

  for (gint y = 0; y < height; ++y)
    {
      src_p = src_data + y * (rowstride / 4);
      dst_p = dst_data + y * (rowstride / 4);

      for (gint x = 0; x < width; ++x)
        {
          unpack_rgba24 (src_p[x], &rgba);
          if (filter_func != NULL)
            filter_func (&rgba, &filtered_rgba, NULL);
          else
            filtered_rgba = rgba;

          dst_p[x] = pack_rgba24 (&filtered_rgba);
        }
    }
}

static void
setup_sample (FilterData *filter_data,
              gint        sample_num)
{
  g_autofree gchar *name = NULL;
  gint width;
  gint height;
  GError *error = NULL;

  g_assert (filter_data != NULL);

  name = g_strdup_printf ("%s/sample%i.jpg", TEST_DATA_DIR, sample_num);

  g_clear_object (&filter_data->src_pixbuf);
  filter_data->src_pixbuf = gdk_pixbuf_new_from_file (name, &error);
  g_assert (GDK_IS_PIXBUF (filter_data->src_pixbuf));

  if (!gdk_pixbuf_get_has_alpha (filter_data->src_pixbuf))
    filter_data->src_pixbuf = gdk_pixbuf_add_alpha (filter_data->src_pixbuf, FALSE, 0.0, 0.0, 0.0);

  gtk_image_set_from_pixbuf (GTK_IMAGE (filter_data->src_img), filter_data->src_pixbuf);

  width = gdk_pixbuf_get_width (filter_data->src_pixbuf);
  height = gdk_pixbuf_get_height (filter_data->src_pixbuf);

  g_clear_object (&filter_data->dst_pixbuf);
  filter_data->dst_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
  filter_pixbuf (filter_data->src_pixbuf, filter_data->dst_pixbuf, filter_data->filter_func);
  gtk_image_set_from_pixbuf (GTK_IMAGE (filter_data->dst_img), filter_data->dst_pixbuf);
}

static void
sample_combo_changed (GtkComboBox *combo,
                      FilterData  *filter_data)
{
  GtkTreeIter iter;
  gint sample_num;

  g_assert (GTK_IS_COMBO_BOX (combo));
  g_assert (filter_data != NULL);

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (filter_data->sample_store), &iter, 0, &sample_num, -1);
      setup_sample (filter_data, sample_num);
    }
}

static void
combo_changed (GtkComboBox *combo,
               FilterData  *filter_data)
{
  GtkTreeIter iter;
  GstyleColorFilter filter;
  GstyleColorFilterFunc filter_func;

  g_assert (GTK_IS_COMBO_BOX (combo));
  g_assert (filter_data != NULL);

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (filter_data->store), &iter, 0, &filter, -1);
      switch (filter)
        {
        case GSTYLE_COLOR_FILTER_NONE:
          filter_func = NULL;
          break;

        case GSTYLE_COLOR_FILTER_ACHROMATOPSIA:
          filter_func = gstyle_color_filter_achromatopsia;
          break;

        case GSTYLE_COLOR_FILTER_ACHROMATOMALY:
          filter_func = gstyle_color_filter_achromatomaly;
          break;

        case GSTYLE_COLOR_FILTER_DEUTERANOPIA:
          filter_func = gstyle_color_filter_deuteranopia;
          break;

        case GSTYLE_COLOR_FILTER_DEUTERANOMALY:
          filter_func = gstyle_color_filter_deuteranomaly;
          break;

        case GSTYLE_COLOR_FILTER_PROTANOPIA:
          filter_func = gstyle_color_filter_protanopia;
          break;

        case GSTYLE_COLOR_FILTER_PROTANOMALY:
          filter_func = gstyle_color_filter_protanomaly;
          break;

        case GSTYLE_COLOR_FILTER_TRITANOPIA:
          filter_func = gstyle_color_filter_tritanopia;
          break;

        case GSTYLE_COLOR_FILTER_TRITANOMALY:
          filter_func = gstyle_color_filter_tritanomaly;
          break;

        case GSTYLE_COLOR_FILTER_WEBSAFE:
          filter_func = gstyle_color_filter_websafe;
          break;

        default:
          g_assert_not_reached ();
        }

      filter_data->filter_func = filter_func;
      filter_pixbuf (filter_data->src_pixbuf, filter_data->dst_pixbuf, filter_func);
      gtk_image_set_from_pixbuf (GTK_IMAGE (filter_data->dst_img), filter_data->dst_pixbuf);
    }
}

static void
test_filter (void)
{
  g_autoptr (GtkBuilder) builder = NULL;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *combo, *sample_combo;
  FilterData filter_data = {0};
  GError *error = NULL;

  gtk_init (NULL, NULL);
  builder = gtk_builder_new ();

  gtk_builder_add_from_file (builder, TEST_DATA_DIR"/gstyle-filter.ui", &error);
  g_assert_no_error (error);

  box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
  filter_data.src_img = GTK_WIDGET (gtk_builder_get_object (builder, "src_img"));
  filter_data.dst_img = GTK_WIDGET (gtk_builder_get_object (builder, "dst_img"));
  setup_sample (&filter_data, 0);

  sample_combo = GTK_WIDGET (gtk_builder_get_object (builder, "sample_combo"));
  filter_data.sample_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "sample_store"));
  gtk_list_store_insert_with_values (filter_data.sample_store, NULL, -1, 0, 0, 1, "Color powders", -1);
  gtk_list_store_insert_with_values (filter_data.sample_store, NULL, -1, 0, 1, 1, "Palette", -1);

  g_signal_connect (sample_combo, "changed", G_CALLBACK (sample_combo_changed), &filter_data);
  gtk_combo_box_set_active (GTK_COMBO_BOX (sample_combo), 0);

  combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo"));
  filter_data.store = GTK_LIST_STORE (gtk_builder_get_object (builder, "store"));
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_NONE, 1, "None", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_ACHROMATOPSIA, 1, "achromatopsia", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_ACHROMATOMALY, 1, "achromatomaly", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_DEUTERANOPIA, 1, "deuteranopia", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_DEUTERANOMALY, 1, "deuteranomaly", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_PROTANOPIA, 1, "protanopia", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_PROTANOMALY, 1, "protanomaly", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_TRITANOPIA, 1, "tritanopia", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_TRITANOMALY, 1, "tritanomaly", -1);
  gtk_list_store_insert_with_values (filter_data.store, NULL, -1, 0, GSTYLE_COLOR_FILTER_WEBSAFE, 1, "websafe", -1);

  g_signal_connect (combo, "changed", G_CALLBACK (combo_changed), &filter_data);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_visible (GTK_WIDGET (window), TRUE);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/filter", test_filter);

  return g_test_run ();
}
