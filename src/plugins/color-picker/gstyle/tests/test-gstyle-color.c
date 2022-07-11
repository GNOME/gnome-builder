/* test-gstyle-color.c
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

#include "gstyle-color.h"
#include "gstyle-color-component.h"
#include "gstyle-cielab.h"
#include "gstyle-color-convert.h"
#include "gstyle-color-item.h"
#include "gstyle-xyz.h"

typedef struct
{
  gdouble l1;
  gdouble a1;
  gdouble b1;

  gdouble l2;
  gdouble a2;
  gdouble b2;

  gdouble delta_e;
} ColorItem;

static ColorItem lab_table[] =
{
  {   0,  100, -100,   0,  100, -100, 0.0 },
  {   0, -100,  100,   0, -100,  100, 0.0 },
  {   0,    0,  100,   0,    0,  100, 0.0 },
  { 100,  100,  100, 100,  100,  100, 0.0 },
  { 100,    0,    0, 100,    0,    0, 0.0 },
  {  53,   80,   67,  47,   67,   47, 8.372140 },
  {  10,   20,   30,  10,   20,   31, 0.518452 },
  {  10,  -10,   10,  11,   -9,    9, 1.184177 },
  { 100, -128,    0,  99, -128,    5, 1.817340 },
  {  75,  -50,    0,  70,  -50,    5, 4.708821 },
  { 0 }
};

/* TODO: use a speific color table with results to compare */
static void
test_deltae (void)
{
  ColorItem *item;
  GstyleCielab lab1, lab2;
  gdouble calc_delta_e, delta_e;

  g_print ("\n");

  for (gint n = 0; n < G_N_ELEMENTS (lab_table); ++n)
    {
      item = &lab_table[n];
      lab1.l = item->l1;
      lab1.a = item->a1;
      lab1.b = item->b1;
      lab2.l = item->l2;
      lab2.a = item->a2;
      lab2.b = item->b2;
      delta_e = item->delta_e;

      calc_delta_e = gstyle_color_delta_e (&lab1, &lab2);
      g_print ("lab(%f, %f, %f) vs lab(%f, %f, %f) deltaE (%f): %f\n",
              lab1.l, lab1.a, lab1.b,
              lab2.l, lab2.a, lab2.b,
              delta_e, calc_delta_e);

      g_assert (delta_e > calc_delta_e - 1e-3 && delta_e < calc_delta_e + 1e-3);
    }
}

#define RGB_INC (1.0 / 255.0)
#define RGB_SAMPLES (255.0 * 255.0 * 255.0)

#define HSV_H_INC (1.0 / 360.0)
#define HSV_SV_INC (1.0 / 100.0)
#define HSV_SAMPLES (360.0 * 100.0 * 100.0)

#define LAB_L_INC (1.0)
#define LAB_AB_INC (1.0)
#define LAB_SAMPLES (100.0 * 256.0 * 256.0)

static void
delta_rgb (void)
{
  GdkRGBA src_rgba = {0.0, 0.0, 0.0, 0.0};
  GdkRGBA dst_rgba = {0.0, 0.0, 0.0, 0.0};
  GdkRGBA max_src_r_rgba, max_src_g_rgba, max_src_b_rgba;
  GdkRGBA max_dst_r_rgba;
  G_GNUC_UNUSED GdkRGBA max_dst_g_rgba, max_dst_b_rgba ;
  GstyleXYZ xyz;
  GstyleXYZ max_r_xyz = {0.0, 0.0, 0.0, 0.0};
  GstyleXYZ max_g_xyz = {0.0, 0.0, 0.0, 0.0};
  GstyleXYZ max_b_xyz = {0.0, 0.0, 0.0, 0.0};
  gdouble r, g, b;
  GdkRGBA delta_rgba;
  gdouble dr_max = 0.0;
  gdouble dg_max = 0.0;
  gdouble db_max = 0.0;
  gdouble dr_min = 0.0;
  gdouble dg_min = 0.0;
  gdouble db_min = 0.0;
  gdouble dr_moy = 0.0;
  gdouble dg_moy = 0.0;
  gdouble db_moy = 0.0;
  gdouble start_time, time;

  start_time = g_get_monotonic_time ();

  max_src_r_rgba.alpha = 0.0;
  max_src_g_rgba.alpha = 0.0;
  max_src_b_rgba.alpha = 0.0;
  max_dst_r_rgba.alpha = 0.0;
  max_dst_g_rgba.alpha = 0.0;
  max_dst_b_rgba.alpha = 0.0;

  /* rgb to xyz */
  for (r = 0.0; r <= 1.0; r += RGB_INC)
    {
      for (g = 0.0; g <= 1.0; g += RGB_INC)
        {
          for (b = 0.0; b <= 1.0; b += RGB_INC)
            {
              src_rgba.red = r;
              src_rgba.green = g;
              src_rgba.blue = b;

              gstyle_color_convert_rgb_to_xyz (&src_rgba, &xyz);
              gstyle_color_convert_xyz_to_rgb (&xyz, &dst_rgba);

              delta_rgba.red = src_rgba.red - dst_rgba.red;
              delta_rgba.green = src_rgba.green - dst_rgba.green;
              delta_rgba.blue = src_rgba.blue - dst_rgba.blue;

              dr_moy += ABS (delta_rgba.red);
              dg_moy += ABS (delta_rgba.green);
              db_moy += ABS (delta_rgba.blue);

              if (ABS (delta_rgba.red) > dr_max)
                {
                  dr_max = ABS (delta_rgba.red);
                  max_src_r_rgba = src_rgba;
                  max_dst_r_rgba = dst_rgba;
                  max_r_xyz = xyz;
                }

              if (ABS (delta_rgba.green) > dg_max)
                {
                  dg_max = ABS (delta_rgba.green);
                  max_src_g_rgba = src_rgba;
                  max_dst_g_rgba = dst_rgba;
                  max_g_xyz = xyz;
                }

              if (ABS (delta_rgba.blue) > db_max)
                {
                  db_max = ABS (delta_rgba.blue);
                  max_src_b_rgba = src_rgba;
                  max_dst_b_rgba = dst_rgba;
                  max_b_xyz = xyz;
                }

              dr_min = MIN (dr_min, ABS (delta_rgba.red));
              dg_min = MIN (dg_min, ABS (delta_rgba.green));
              db_min = MIN (db_min, ABS (delta_rgba.blue));
            }
        }
    }

  time = g_get_monotonic_time () - start_time;
  dr_moy /= 255.0 * 255.0 * 255.0;
  dg_moy /= 255.0 * 255.0 * 255.0;
  db_moy /= 255.0 * 255.0 * 255.0;

  g_print ("\nRGB -> XYZ -> RGB:\n");
  g_print ("red:\n\tΔmax: %f%% (normalized/255: %f)\n\tΔmin: %f%% (normalized/255: %f)\n\tΔmoy: %f%%(normalized/255: %f)\n\n",
          dr_max, dr_max * 255.0, dr_min, dr_min * 255.0, dr_moy, dr_moy * 255.0);
  g_print ("green:\n\tΔmax: %f%% (normalized/255: %f)\n\tΔmin: %f%% (normalized/255: %f)\n\tΔmoy: %f%%(normalized/255: %f)\n\n",
          dg_max, dg_max * 255.0, dg_min, dg_min * 255.0, dg_moy, dg_moy * 255.0);
  g_print ("blue:\n\tΔmax: %f%% (normalized/255: %f)\n\tΔmin: %f%% (normalized/255: %f)\n\tΔmoy: %f%%(normalized/255: %f)\n\n",
          db_max, db_max * 255.0, db_min, db_min * 255.0, db_moy, db_moy * 255.0);

  g_print ("time micro sec: %f (per sample:%f) sec: %f\n\n", time, time / RGB_SAMPLES, time / 1.0e6);

  {
    g_autofree gchar *src_str = gdk_rgba_to_string (&max_src_r_rgba);
    g_autofree gchar *dst_str = gdk_rgba_to_string (&max_dst_r_rgba);

    g_print ("max red src rgba:%s dst rgba:%s xyz:(%f, %f, %f)\n", src_str, dst_str, max_r_xyz.x, max_r_xyz.y, max_r_xyz.z);
  }

  {
    g_autofree gchar *src_str = gdk_rgba_to_string (&max_src_g_rgba);
    g_autofree gchar *dst_str = gdk_rgba_to_string (&max_dst_g_rgba);

    g_print ("max green src rgba:%s dst rgba:%s xyz:(%f, %f, %f)\n", src_str, dst_str, max_g_xyz.x, max_g_xyz.y, max_g_xyz.z);
  }

  {
    g_autofree gchar *src_str = gdk_rgba_to_string (&max_src_b_rgba);
    g_autofree gchar *dst_str = gdk_rgba_to_string (&max_dst_b_rgba);

    g_print ("max blue src rgba:%s dst rgba:%s xyz:(%f, %f, %f)\n", src_str, dst_str, max_b_xyz.x, max_b_xyz.y, max_b_xyz.z);
  }
}

static void
delta_hsv (void)
{
  gdouble src_h, src_s, src_v;
  gdouble dst_h, dst_s, dst_v;
  GstyleXYZ xyz;
  gdouble dh = 0.0;
  gdouble ds = 0.0;
  gdouble dv = 0.0;
  gdouble dh_max = 0.0;
  gdouble ds_max = 0.0;
  gdouble dv_max = 0.0;
  gdouble dh_min = 0.0;
  gdouble ds_min = 0.0;
  gdouble dv_min = 0.0;
  gdouble dh_moy = 0.0;
  gdouble ds_moy = 0.0;
  gdouble dv_moy = 0.0;
  gdouble start_time, time;
  gdouble max_src_hsv_h = 0.0;
  gdouble max_src_hsv_s = 0.0;
  gdouble max_src_hsv_v = 0.0;
  gdouble max_dst_hsv_h = 0.0;
  gdouble max_dst_hsv_s = 0.0;
  gdouble max_dst_hsv_v = 0.0;

  start_time = g_get_monotonic_time ();

  /* rgb to xyz */
  for (src_h = 0.0; src_h <= 1.0; src_h += HSV_H_INC)
    {
      for (src_s = 0.0; src_s <= 1.0; src_s += HSV_SV_INC)
        {
          for (src_v = 0.0; src_v <= 1.0; src_v += HSV_SV_INC)
            {
              gstyle_color_convert_hsv_to_xyz (src_h, src_s, src_v, &xyz);
              gstyle_color_convert_xyz_to_hsv (&xyz, &dst_h, &dst_s, &dst_v);

              dh = dst_h - src_h;
              ds = dst_s - src_s;
              dv = dst_v - src_v;

              dh_moy += ABS (dh);
              ds_moy += ABS (ds);
              dv_moy += ABS (dv);

              if (ABS(dh) > dh_max)
                {
                  dh_max = ABS (dh);
                  max_src_hsv_h = src_h;
                  max_src_hsv_s = src_s;
                  max_src_hsv_v = src_v;
                  max_dst_hsv_h = dst_h;
                  max_dst_hsv_s = dst_s;
                  max_dst_hsv_v = dst_v;
                }

              if (ABS(ds) > ds_max)
                {
                  ds_max = ABS (ds);
                  max_src_hsv_h = src_h;
                  max_src_hsv_s = src_s;
                  max_src_hsv_v = src_v;
                  max_dst_hsv_h = dst_h;
                  max_dst_hsv_s = dst_s;
                  max_dst_hsv_v = dst_v;
                }

              if (ABS(dv) > dv_max)
                {
                  dv_max = ABS (dv);
                  max_src_hsv_h = src_h;
                  max_src_hsv_s = src_s;
                  max_src_hsv_v = src_v;
                  max_dst_hsv_h = dst_h;
                  max_dst_hsv_s = dst_s;
                  max_dst_hsv_v = dst_v;
                }

              dh_min = MIN (dh_min, ABS (dh));
              ds_min = MIN (ds_min, ABS (ds));
              dv_min = MIN (dv_min, ABS (dv));
            }
        }
    }

  time = g_get_monotonic_time () - start_time;
  dh_moy /= 360.0 * 100.0 * 100.0;
  ds_moy /= 255.0 * 100.0 * 100.0;
  dv_moy /= 255.0 * 100.0 * 100.0;

  g_print ("\nHSV -> XYZ -> HSV:\n");
  g_print ("hue:\n\tΔmax: %f%% (norm/360:%f)\n\tΔmin:%f%% (norm/360:%f)\n\tΔmoy:%f%% (norm/360:%f)\n\n",
          dh_max, dh_max * 360.0, dh_min, dh_min * 360.0, dh_moy, dh_moy * 360.0);
  g_print ("saturation:\n\tΔmax:%f%% (norm/100:%f)\n\tΔmin:%f%% (norm/100:%f)\n\tΔmoy:%f%% (norm/100:%f)\n\n",
          ds_max, ds_max * 100.0, ds_min, ds_min * 100.0, ds_moy, ds_moy * 100.0);
  g_print ("value:\n\tΔmax:%f%% (norm/100:%f)\n\tΔmin:%f%% (norm/100:%f)\n\tΔmoy:%f%% (norm/100:%f)\n\n",
          dv_max, dv_min * 100.0, dv_min, dv_min * 100.0, dv_moy, dv_moy * 100.0);

  g_print ("time micro sec: %f (per sample:%f) sec: %f\n\n", time, time / HSV_SAMPLES, time / 1.0e6);

  g_print ("max hue src hsv(%f,%f,%f) dst hsv(%f,%f,%f)\n",
          max_src_hsv_h, max_src_hsv_s, max_src_hsv_v,
          max_dst_hsv_h, max_dst_hsv_s, max_dst_hsv_v);
}

static void
delta_lab (void)
{
  GstyleXYZ xyz;
  GstyleCielab src_lab;
  GstyleCielab dst_lab;
  gdouble dl = 0.0;
  gdouble da = 0.0;
  gdouble db = 0.0;
  gdouble dl_max = 0.0;
  gdouble da_max = 0.0;
  gdouble db_max = 0.0;
  gdouble dl_min = 0.0;
  gdouble da_min = 0.0;
  gdouble db_min = 0.0;
  gdouble dl_moy = 0.0;
  gdouble da_moy = 0.0;
  gdouble db_moy = 0.0;
  gdouble start_time, time;

  start_time = g_get_monotonic_time ();

  /* rgb to xyz */
  for (src_lab.l = 0.0; src_lab.l <= 100.0; src_lab.l += LAB_L_INC)
    {
      for (src_lab.a = -128.0; src_lab.a <= 128.0; src_lab.a += LAB_AB_INC)
        {
          for (src_lab.b = -128.0; src_lab.b <= 128.0; src_lab.b += LAB_AB_INC)
            {
              gstyle_color_convert_cielab_to_xyz (&src_lab, &xyz);
              gstyle_color_convert_xyz_to_cielab (&xyz, &dst_lab);

              dl = src_lab.l - dst_lab.l;
              da = src_lab.a - dst_lab.a;
              db = src_lab.b - dst_lab.b;

              dl_moy += ABS (dl);
              da_moy += ABS (da);
              db_moy += ABS (db);

              dl_max = MAX (dl_max, ABS (dl));
              da_max = MAX (da_max, ABS (da));
              db_max = MAX (db_max, ABS (db));

              dl_min = MIN (dl_min, ABS (dl));
              da_min = MIN (da_min, ABS (da));
              db_min = MIN (db_min, ABS (db));
            }
        }
    }

  time = g_get_monotonic_time () - start_time;
  dl_moy /= 100.0 * 256.0 * 256.0;
  da_moy /= 100.0 * 256.0 * 256.0;
  db_moy /= 100.0 * 256.0 * 256.0;

  g_print ("\nLAB -> XYZ -> LAB:\n");
  g_print ("L* in [0, 100]:\n\tΔl max: %f%%\n\tΔl min:%f%%\n\tΔl moy:%f%%\n\n",
          dl_max, dl_min, dl_moy);
  g_print ("a* in [-128, +128]:\n\tΔa max: %f%%\n\tΔa min:%f\n\tΔa moy:%f%%\n\n",
          da_max, da_min, da_moy);
  g_print ("b* in [-128, +128]:\n\tΔb max: %f%%\n\tΔb min:%f%%\n\tΔb moy:%f%%\n\n",
          db_max, db_min, db_moy);

  g_print ("time micro sec: %f (per sample:%f) sec: %f\n\n", time, time / LAB_SAMPLES, time / 1.0e6);
}

static void
test_conversion (void)
{
  delta_rgb ();
  delta_hsv ();
  delta_lab ();
}

int
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gstyle/deltaE", test_deltae);
  g_test_add_func ("/Gstyle/conversion", test_conversion);

  return g_test_run ();
}
