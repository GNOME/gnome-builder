/* gstyle-color-convert.c
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "gstyle-color-convert.h"

#define _2PI          6.2831853071795864769252867665590057683943387987502
#define PI_d_6        0.523598775598298873077107230546583814032861566562516
#define PI_d_30       0.104719755119659774615421446109316762806572313312503
#define _63PI_d_180   1.099557428756427633461925184147826009469009289781285
#define _180_d_PI     57.29577951308232087679815481410517033240547246656442
#define _25_pow_7     6103515625
#define _30PI_d_180   0.523598775598298873077107230546583814032861566562516
#define _16_d_116     0.137931034

#define one_third     0.333333333333333333333333333333333333333333333333333
#define two_third     0.666666666666666666666666666666666666666666666666666

#define D65_xref      0.95047
#define D65_yref      1.0
#define D65_zref      1.08883

/* DeltaE algorithm described at:
 * http://www.ece.rochester.edu/~gsharma/ciede2000/ciede2000noteCRNA.pdf
 *
 */

/* pow_1_24 and pow_24 are adapted version from babl, published under LGPL */
/* babl - dynamically extendable universal pixel conversion library.
 * Copyright 2012, Red Hat, Inc.
 */

/* Chebychev polynomial terms for x^(5/12) expanded around x=1.5
 * Non-zero terms calculated via
 * NIntegrate[(2/Pi)*ChebyshevT[N,u]/Sqrt [1-u^2]*((u+3)/2)^(5/12),{u,-1,1}, PrecisionGoal->20, WorkingPrecision -> 100]
 * Zeroth term is similar except it uses 1/pi rather than 2/pi.
 */
static const double Cn[] = {
  1.1758200232996901923,
  0.16665763094889061230,
  -0.0083154894939042125035,
  0.00075187976780420279038,
  -0.000083240178519391795367,
  0.000010229209410070008679,
  -1.3401001466409860246e-6,
  1.8333422241635376682e-7,
  -2.5878596761348859722e-8
};

/* Returns x^(5/12) for x in [1,2) */
static inline gdouble
pow512norm (gdouble x)
{
  gdouble Tn[9];
  gdouble u;

  u = 2.0*x - 3.0;
  Tn[0] = 1.0;
  Tn[1] = u;
  Tn[2] = 2*u*Tn[2-1] - Tn[2-2];
  Tn[3] = 2*u*Tn[3-1] - Tn[3-2];
  Tn[4] = 2*u*Tn[4-1] - Tn[4-2];
  Tn[5] = 2*u*Tn[5-1] - Tn[5-2];
  Tn[6] = 2*u*Tn[6-1] - Tn[6-2];

  return Cn[0]*Tn[0] + Cn[1]*Tn[1] + Cn[2]*Tn[2] + Cn[3]*Tn[3] + Cn[4]*Tn[4] + Cn[5]*Tn[5] + Cn[6]*Tn[6];
}

/* Precalculated (2^N) ^ (5 / 12) */
static const gdouble pow2_512[12] =
{
  1.0,
  1.3348398541700343678,
  1.7817974362806785482,
  2.3784142300054420538,
  3.1748021039363991669,
  4.2378523774371812394,
  5.6568542494923805819,
  7.5509945014535482244,
  1.0079368399158985525e1,
  1.3454342644059433809e1,
  1.7959392772949968275e1,
  2.3972913230026907883e1
};

/* Returns x^(1/2.4) == x^(5/12) */
static inline gdouble
pow_1_24 (gdouble x)
{
  gdouble s;
  gint iexp;
  div_t qr = {0};

  s = frexp (x, &iexp);
  s *= 2.0;
  iexp -= 1;

  qr = div (iexp, 12);
  if (qr.rem < 0)
    {
      qr.quot -= 1;
      qr.rem += 12;
    }

  return ldexp (pow512norm (s) * pow2_512[qr.rem], 5 * qr.quot);
}

/* Chebychev polynomial terms for x^(7/5) expanded around x=1.5
 * Non-zero terms calculated via
 * NIntegrate[(2/Pi)*ChebyshevT[N,u]/Sqrt [1-u^2]*((u+3)/2)^(7/5),{u,-1,1}, PrecisionGoal->20, WorkingPrecision -> 100]
 * Zeroth term is similar except it uses 1/pi rather than 2/pi.
 */
static const gdouble iCn[] =
{
  1.7917488588043277509,
  0.82045614371976854984,
  0.027694100686325412819,
  -0.00094244335181762134018,
  0.000064355540911469709545,
  -5.7224404636060757485e-6,
  5.8767669437311184313e-7,
  -6.6139920053589721168e-8,
  7.9323242696227458163e-9
};

/* Returns x^(7/5) for x in [1,2) */
static inline gdouble
pow75norm (gdouble x)
{
  gdouble Tn[9];
  gdouble u;

  u = 2.0*x - 3.0;
  Tn[0] = 1.0;
  Tn[1] = u;
  Tn[2] = 2*u*Tn[2-1] - Tn[2-2];
  Tn[3] = 2*u*Tn[3-1] - Tn[3-2];
  Tn[4] = 2*u*Tn[4-1] - Tn[4-2];
  Tn[5] = 2*u*Tn[5-1] - Tn[5-2];

  return iCn[0]*Tn[0] + iCn[1]*Tn[1] + iCn[2]*Tn[2] + iCn[3]*Tn[3] + iCn[4]*Tn[4] + iCn[5]*Tn[5];
}

/* Precalculated (2^N) ^ (7 / 5) */
static const gdouble pow2_75[5] =
{
  1.0,
  2.6390158215457883983,
  6.9644045063689921093,
  1.8379173679952558018e+1,
  4.8502930128332728543e+1
};

/* Returns x^2.4 == x * x ^1.4 == x * x^(7/5) */
static inline gdouble
pow_24 (gdouble x)
{
  gdouble s;
  gint iexp;
  div_t qr = {0};

  s = frexp (x, &iexp);
  s *= 2.0;
  iexp -= 1;

  qr = div (iexp, 5);
  if (qr.rem < 0)
    {
      qr.quot -= 1;
      qr.rem += 5;
    }

  return x * ldexp (pow75norm (s) * pow2_75[qr.rem], 7 * qr.quot);
}

static inline gboolean
fix_rgb_bounds (GdkRGBA *rgba)
{
  gdouble r, g, b;
  gboolean res = TRUE;

  r = rgba->red;
  g = rgba->green;
  b = rgba->blue;

  if (r < 0.0)
    {
      rgba->red = 0.0;
      res = FALSE;
    }
  else if (r > 1.0)
    {
      rgba->red = 1.0;
      res = FALSE;
    }

  if (g < 0.0)
    {
      rgba->green = 0.0;
      res = FALSE;
    }
  else if (g > 1.0)
    {
      rgba->green = 1.0;
      res = FALSE;
    }

    if (b < 0.0)
    {
      rgba->blue = 0.0;
      res = FALSE;
    }
  else if (b > 1.0)
    {
      rgba->blue = 1.0;
      res = FALSE;
    }

  return res;
}

static inline void
gstyle_color_convert_rgb_to_srgb (GdkRGBA *rgba,
                                  gdouble *red,
                                  gdouble *green,
                                  gdouble *blue)
{
  /* rgba and srgb values range [0, 1] */

  *red = (rgba->red > 0.04045) ? pow_24 (((rgba->red + 0.055) / 1.055)) : rgba->red / 12.92;
  *green = (rgba->green > 0.04045) ? pow_24 (((rgba->green + 0.055) / 1.055)) : rgba->green / 12.92;
  *blue = (rgba->blue > 0.04045) ? pow_24 (((rgba->blue + 0.055) / 1.055)) : rgba->blue / 12.92;
}

static inline void
gstyle_color_convert_srgb_to_rgb (gdouble  red,
                                  gdouble  green,
                                  gdouble  blue,
                                  GdkRGBA *rgba)
{
  /* rgba and srgb values range [0, 1] */

  rgba->red = (red > 0.0031308) ? (pow_1_24 (red) * 1.055) - 0.055 : red * 12.92;
  rgba->green = (green > 0.0031308) ? (pow_1_24 (green) * 1.055) - 0.055 : green * 12.92;
  rgba->blue = (blue > 0.0031308) ? (pow_1_24 (blue) * 1.055) - 0.055 : blue * 12.92;

  fix_rgb_bounds (rgba);
}

static inline void
gstyle_color_convert_srgb_to_xyz (gdouble    red,
                                  gdouble    green,
                                  gdouble    blue,
                                  GstyleXYZ *xyz)
{
  /* srgb range [0, 1] x [0, 0.9505] y [0, 1] z [0, 1.08883] Observer= 2°, Illuminant= D65 */

  xyz->x = (red * 0.4124564 + green * 0.3575761 + blue * 0.1804375);
  xyz->y = (red * 0.2126729 + green * 0.7151522 + blue * 0.0721750);
  xyz->z = (red * 0.0193339 + green * 0.1191920 + blue * 0.9503041);

  //fix_xyz_bounds (xyz);
}

static inline void
gstyle_color_convert_xyz_to_srgb (GstyleXYZ *xyz,
                                  gdouble   *red,
                                  gdouble   *green,
                                  gdouble   *blue)
{
  /* srgb range [0, 1] x [0, 0.9505] y [0, 1] z [0, 1.08883] Observer= 2°, Illuminant= D65 */

  *red   = xyz->x *  3.2404542 + xyz->y * -1.5371385 + xyz->z * -0.4985314;
  *green = xyz->x * -0.9692660 + xyz->y *  1.8760108 + xyz->z *  0.0415560;
  *blue  = xyz->x *  0.0556434 + xyz->y * -0.2040259 + xyz->z *  1.0572252;
}

inline void
gstyle_color_convert_cielab_to_xyz (GstyleCielab *lab,
                                    GstyleXYZ    *xyz)
{
  gdouble tmp_x, tmp_y, tmp_z;
  gdouble pow3_x, pow3_y, pow3_z;

  tmp_y = (lab->l + 16.0 ) / 116.0;
  tmp_x = lab->a / 500.0 + tmp_y;
  tmp_z = tmp_y - lab->b / 200.0;

  /* far faster than pow (x, 3) */
  pow3_x = tmp_x * tmp_x * tmp_x;
  pow3_y = tmp_y * tmp_y * tmp_y;
  pow3_z = tmp_z * tmp_z * tmp_z;

  tmp_x = (pow3_x > 0.008856) ? pow3_x : (tmp_x - _16_d_116) / 7.787;
  tmp_y = (pow3_y > 0.008856) ? pow3_y : (tmp_y - _16_d_116) / 7.787;
  tmp_z = (pow3_z > 0.008856) ? pow3_z : (tmp_z - _16_d_116) / 7.787;

  xyz->x = tmp_x * D65_xref;
  xyz->y = tmp_y * D65_yref;
  xyz->z = tmp_z * D65_zref;
}

/* fastpow (x, 0.333333333) */
inline void
gstyle_color_convert_xyz_to_cielab (GstyleXYZ    *xyz,
                                    GstyleCielab *lab)
{
  /* Observer= 2°, Illuminant= D65 */
  gdouble x, y, z;

  x = xyz->x / D65_xref;
  y = xyz->y / D65_yref;
  z = xyz->z / D65_zref;

  x = (x > 0.008856) ? cbrt (x) : (x * 7.787) + _16_d_116;
  y = (y > 0.008856) ? cbrt (y) : (y * 7.787) + _16_d_116;
  z = (z > 0.008856) ? cbrt (z) : (z * 7.787) + _16_d_116;

  lab->l = y * 116.0 - 16.0;
  lab->a = (x - y) * 500.0;
  lab->b = (y - z) * 200.0;
}

/**
 * gstyle_color_convert_rgb_to_hsl:
 * @rgba: a #GdkRGBA struct.
 * @hue: (out): The hue component of a hsl color in range  [0.0-360.0[
 * @saturation: (out): The saturation component of a hsl color in range [0.0-100.0]
 * @lightness: (out): The lightness component of a hsl color in range [0.0-100.0]
 *
 * Convert rgb components to HSL ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_rgb_to_hsl (GdkRGBA *rgba,
                                 gdouble *hue,
                                 gdouble *saturation,
                                 gdouble *lightness)
{
  gdouble tmp_hue = 0.0;
  gdouble tmp_saturation = 0.0;
  gdouble tmp_lightness;

  gdouble red = rgba->red;
  gdouble green = rgba->green;
  gdouble blue = rgba->blue;

  gdouble min;
  gdouble max;
  gdouble d;
  gdouble max_min;

  if (red > green)
    {
      max = (red > blue) ? red : blue;
      min = (green < blue) ? green : blue;
    }
  else
    {
      max = (green > blue) ? green : blue;
      min = (red < blue) ? red : blue;
    }

  max_min = max + min;
  tmp_lightness = max_min / 2.0;
  if (max != min)
    {
      d = max - min;
      tmp_saturation = (tmp_lightness > 0.5) ? d / (2.0 - max - min) : d / max_min;
      if (max == red)
        tmp_hue = (green - blue) / d + (green < blue ? 6.0 : 0.0);
      else if (max == green)
        tmp_hue = (blue - red) / d + 2.0;
      else
        tmp_hue = (red - green) / d + 4.0;
    }

  if (hue != NULL)
    *hue = tmp_hue * 60.0;

  if (saturation != NULL)
    *saturation = tmp_saturation * 100.0;

  if (lightness != NULL)
    *lightness = tmp_lightness * 100.0;
}

static inline gdouble
hue2rgb (gdouble m1,
         gdouble m2,
         gdouble hue)
{
  while (hue < 0.0)
    hue += 360.0;

  while (hue > 360.0)
    hue -= 360.0;

  if (hue < 60.0)
    return m1 + (m2 - m1) * hue / 60.0;

  if (hue < 180.0)
    return m2;

  if (hue < 240.0)
    return m1 + (m2 - m1) * (240.0 - hue) / 60.0;

  return m1;
}

/**
 * gstyle_color_convert_hsl_to_rgb:
 * @hue: The hue component of a hsl color in range  [0.0-360.0[
 * @saturation: The saturation component of a hsl color in range [0.0-100.0]
 * @lightness: The lightness component of a hsl color in range [0.0-100.0]
 * @rgba: a #GdkRGBA.
 *
 * Convert RGB components to HSL ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_hsl_to_rgb (gdouble   hue,
                                 gdouble   saturation,
                                 gdouble   lightness,
                                 GdkRGBA  *rgba)
{
  gdouble m1;
  gdouble m2;

  if (saturation == 0.0)
    rgba->red = rgba->green = rgba->blue = lightness;
  else
    {
      m2 = (lightness > 0.5) ? lightness + saturation - (lightness * saturation) : lightness * (1.0 + saturation);
      m1 = 2.0 * lightness - m2;

      rgba->red = hue2rgb (m1, m2, hue + 120.0);
      rgba->green = hue2rgb (m1, m2, hue);
      rgba->blue = hue2rgb (m1, m2, hue - 120.0);
    }
}

/**
 * gstyle_color_convert_hsv_to_rgb:
 * @hue: The hue component of a hsv color in range  [0.0-1.0[
 * @saturation: The saturation component of a hsv color in range [0.0-1.0]
 * @value: The value component of a hsv color in range [0.0-1.0]
 * @rgba: a #GdkRGBA.
 *
 * Convert HSV components to RGB ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_hsv_to_rgb (gdouble   hue,
                                 gdouble   saturation,
                                 gdouble   value,
                                 GdkRGBA  *rgba)
{
  gdouble f;
  gdouble p;
  gdouble q;
  gdouble t;
  gint i;

  if (saturation == 0.0)
    rgba->red = rgba->green = rgba->blue = value;
  else
    {
      hue *= 6.0;
      if (hue == 6.0)
        hue = 0.0;

      i = (int)hue;
      f = hue - i;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));

      switch (i)
        {
        case 0:
          rgba->red = value;
          rgba->green = t;
          rgba->blue = p;
          break;

        case 1:
          rgba->red = q;
          rgba->green = value;
          rgba->blue = p;
          break;

        case 2:
          rgba->red = p;
          rgba->green = value;
          rgba->blue = t;
          break;

        case 3:
          rgba->red = p;
          rgba->green = q;
          rgba->blue = value;
          break;

        case 4:
          rgba->red = t;
          rgba->green = p;
          rgba->blue = value;
          break;

        case 5:
          rgba->red = value;
          rgba->green = p;
          rgba->blue = q;
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

/**
 * gstyle_color_convert_rgb_to_xyz:
 * @rgba: An #GdkRGBA.
 * @xyz: a #GstyleXYZ.
 *
 * Convert RGB components to XYZ ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_rgb_to_xyz (GdkRGBA   *rgba,
                                 GstyleXYZ *xyz)
{
  gdouble srgb_red, srgb_green, srgb_blue;

  gstyle_color_convert_rgb_to_srgb (rgba, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_xyz (srgb_red, srgb_green, srgb_blue, xyz);
}

/**
 * gstyle_color_convert_rgb_to_hsv:
 * @rgba: An #GdkRGBA.
 * @hue: (out): The hue component of a hsv color in range  [0.0-1.0[
 * @saturation: (out): The saturation component of a hsv color in range [0.0-1.0]
 * @value: (out): The value component of a hsv color in range [0.0-1.0]
 *
 * Convert RGB components to HSV ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_rgb_to_hsv (GdkRGBA *rgba,
                                 gdouble *hue,
                                 gdouble *saturation,
                                 gdouble *value)
{
  gdouble vmin, vmax, delta;
  gdouble d_red, d_green, d_blue;
  gdouble delta_d_2;

  if (rgba->red > rgba->green)
    {
      vmax = (rgba->red > rgba->blue) ? rgba->red : rgba->blue;
      vmin = (rgba->green < rgba->blue) ? rgba->green : rgba->blue;
    }
  else
    {
      vmax = (rgba->green > rgba->blue) ? rgba->green : rgba->blue;
      vmin = (rgba->red < rgba->blue) ? rgba->red : rgba->blue;
    }

  delta = vmax - vmin;
  delta_d_2 = delta / 2.0;

  *value = vmax;

  if (delta < 1e-20 )
    *hue = *saturation = 0.0;
  else
    {
      *saturation = delta / vmax;

      d_red   = ((vmax - rgba->red)   / 6.0 + delta_d_2) / delta;
      d_green = ((vmax - rgba->green) / 6.0 + delta_d_2) / delta;
      d_blue  = ((vmax - rgba->blue)  / 6.0 + delta_d_2) / delta;

      if (vmax == rgba->red)
         *hue = d_blue - d_green;
      else if (vmax == rgba->green)
         *hue = one_third + d_red - d_blue;
      else if (vmax == rgba->blue)
        *hue = two_third + d_green - d_red;

      if (*hue < 0.0)
        *hue += 1.0;
      else if (*hue > 1.0 )
        *hue -= 1.0;
    }
}

/**
 * gstyle_color_convert_rgb_to_cielab:
 * @rgba: An #GdkRGBA.
 * @lab: (out): a #GstyleCieLab struct.
 *
 * Convert RGB components to CIELAB ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_rgb_to_cielab (GdkRGBA      *rgba,
                                    GstyleCielab *lab)
{
  gdouble srgb_red, srgb_green, srgb_blue;
  GstyleXYZ xyz;

  gstyle_color_convert_rgb_to_srgb (rgba, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_xyz (srgb_red, srgb_green, srgb_blue, &xyz);
  gstyle_color_convert_xyz_to_cielab (&xyz, lab);
}

/**
 * gstyle_color_convert_cielab_to_rgb:
 * @lab: a #GstyleCieLab struct.
 * @rgba: (out): An #GdkRGBA.
 *
 * Convert CIELAB components to RGB ones.
 * The alpha component is not used because it doesn't change in the conversion.
 *
 */
void
gstyle_color_convert_cielab_to_rgb (GstyleCielab *lab,
                                    GdkRGBA      *rgba)
{
  gdouble srgb_red, srgb_green, srgb_blue;
  GstyleXYZ xyz;

  gstyle_color_convert_cielab_to_xyz (lab, &xyz);
  gstyle_color_convert_xyz_to_srgb (&xyz, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_rgb (srgb_red, srgb_green, srgb_blue, rgba);
}

inline void
gstyle_color_convert_xyz_to_rgb (GstyleXYZ *xyz,
                                 GdkRGBA   *rgba)
{
  gdouble srgb_red, srgb_green, srgb_blue;

  gstyle_color_convert_xyz_to_srgb (xyz, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_rgb (srgb_red, srgb_green, srgb_blue, rgba);
}

/**
 * gstyle_color_convert_hsv_to_xyz:
 * @hue:  The hue component of a hsv color in range  [0.0-1.0[
 * @saturation: The saturation component of a hsv color in range [0.0-1.0]
 * @value: The value component of a hsv color in range [0.0-1.0]
 * @xyz: (out): An #GstyleXYZ.
 *
 * Convert HSV components to XYZ ones.
 *
 */
void
gstyle_color_convert_hsv_to_xyz (gdouble    hue,
                                 gdouble    saturation,
                                 gdouble    value,
                                 GstyleXYZ *xyz)
{
  gdouble srgb_red, srgb_green, srgb_blue;
  GdkRGBA rgba;

  gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &rgba);
  gstyle_color_convert_rgb_to_srgb (&rgba, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_xyz (srgb_red, srgb_green, srgb_blue, xyz);
}

/**
 * gstyle_color_convert_xyz_to_hsv:
 * @xyz: An #GstyleXYZ.
 * @hue: (out): The hue component of a hsv color in range  [0.0-1.0[
 * @saturation: (out): The saturation component of a hsv color in range [0.0-1.0]
 * @value: (out): The value component of a hsv color in range [0.0-1.0]
 *
 * Convert XYZ components to HSV ones.
 *
 */
void
gstyle_color_convert_xyz_to_hsv (GstyleXYZ *xyz,
                                 gdouble   *hue,
                                 gdouble   *saturation,
                                 gdouble   *value)
{
  GdkRGBA rgba;
  gdouble srgb_red, srgb_green, srgb_blue;

  gstyle_color_convert_xyz_to_srgb (xyz, &srgb_red, &srgb_green, &srgb_blue);
  gstyle_color_convert_srgb_to_rgb (srgb_red, srgb_green, srgb_blue, &rgba);
  gstyle_color_convert_rgb_to_hsv (&rgba, hue, saturation, value);
}

/**
 * gstyle_color_delta_e:
 * @lab1: a #GstyleCielab.
 * @lab2: a #GstyleCielab.
 *
 * Compute the color difference between lab1 and lab2,
 * based on the deltaE CIEDE2000 formula.
 *
 * Returns: the deltaE value.
 *
 */
gdouble
gstyle_color_delta_e (GstyleCielab *lab1,
                      GstyleCielab *lab2)
{
  gdouble ap1, Cp1, hp1, Cab1;
  gdouble ap2, Cp2, hp2, Cab2;
  gdouble Cab;

  gdouble Cp1Cp2;
  gdouble Cab_pow_7, G;

  gdouble DLp, DCp, DHp;
  gdouble dhp, Lp, Cp, hp;

  gdouble T;
  gdouble Dtheta_rad;
  gdouble Rc, RT;

  gdouble _50Lp_pow2;
  gdouble SL, SC, SH;

  gdouble lab1_bb = lab1->b * lab1->b;
  gdouble lab2_bb = lab2->b * lab2->b;

  Cab1 = sqrt (lab1->a * lab1->a + lab1_bb);
  Cab2 = sqrt (lab2->a * lab2->a + lab2_bb);
  Cab = (Cab1 + Cab2) / 2.0;
  Cab_pow_7 = pow (Cab, 7);

  G = 0.5 * (1.0 - sqrt (Cab_pow_7 / (Cab_pow_7 + _25_pow_7)));

  ap1 = (1.0 + G) * lab1->a;
  ap2 = (1.0 + G) * lab2->a;

  Cp1 = sqrt (ap1 * ap1 + lab1_bb);
  Cp2 = sqrt (ap2 * ap2 + lab2_bb);
  Cp1Cp2 = (Cp1 * Cp2);

  if (ap1 == 0 && lab1->b == 0)
    hp1 = 0.0;
  else
    {
      hp1 = atan2 (lab1->b, ap1);
      if (hp1 < 0)
        hp1 += _2PI;
    }

  if (ap2 == 0 && lab2->b == 0)
    hp2 = 0.0;
  else
    {
      hp2 = atan2 (lab2->b, ap2);
      if (hp2 < 0)
        hp2 += _2PI;
    }

  DLp = (lab2->l - lab1->l);
  DCp = (Cp2 - Cp1);

  if (Cp1Cp2 == 0.0)
    {
      dhp = 0.0;
      DHp = 0.0;

      hp = hp1 + hp2;
    }
  else
    {
      dhp = (hp2 - hp1);
      if (dhp > G_PI)
        dhp -= _2PI;

      if (dhp < -G_PI)
        dhp += _2PI;

      DHp = 2.0 * sqrt (Cp1Cp2) * sin (dhp / 2.0);

      hp = (hp1 + hp2) / 2.0;
      if (fabs (hp1 - hp2) > G_PI)
        hp -= G_PI;

      if (hp < 0)
        hp += _2PI;
    }

  Lp = (lab1->l + lab2->l) / 2.0;
  Cp = (Cp1 + Cp2) / 2.0;

  T = 1.0 - 0.17 * cos (hp - PI_d_6) +
      0.24 * cos (2.0 * hp) +
      0.32 * cos (3.0 * hp + PI_d_30) -
      0.20 * cos (4.0 * hp - _63PI_d_180);

  Dtheta_rad = _30PI_d_180 * exp (-pow (((_180_d_PI * hp - 275.0) / 25.0), 2.0));

  Rc = 2.0 * sqrt (pow (Cp, 7.0) / (pow (Cp, 7.0) + _25_pow_7));

  _50Lp_pow2 = (Lp - 50.0) * (Lp - 50.0);
  SL = 1.0 + (0.015 * _50Lp_pow2 / sqrt (20.0 + _50Lp_pow2));
  SC = 1.0 + 0.045 * Cp;
  SH = 1.0 + 0.015 * Cp * T;

  RT = -sin (2.0 * Dtheta_rad) * Rc;

  return sqrt( pow ((DLp / SL), 2.0) +
         pow((DCp / SC), 2.0) +
         pow((DHp / SH), 2.0) +
         RT * (DCp / SC) * (DHp / SH));
}
