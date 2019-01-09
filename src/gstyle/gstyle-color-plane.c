/* gstyle-color-plane.c
 *
 * based on : gtk-color-plane
 *   GTK - The GIMP Toolkit
 *   Copyright 2012 Red Hat, Inc.
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

#define G_LOG_DOMAIN "gstyle-color-plane"

#include <math.h>
#include <cairo/cairo.h>
#include <glib/gi18n.h>

#include "gstyle-cielab.h"
#include "gstyle-color-convert.h"
#include "gstyle-css-provider.h"
#include "gstyle-utils.h"

#include "gstyle-color-plane.h"

typedef struct _ComputeData
{
  gint     width;
  gint     height;
  gint     stride;
  guint32 *buffer;

  gdouble  x_factor;
  gdouble  y_factor;
  gdouble  lab_x_factor;
  gdouble  lab_y_factor;
  gdouble  lab_l_factor;
} ComputeData;

typedef enum _ColorSpaceId
{
  COLOR_SPACE_RGB,
  COLOR_SPACE_CIELAB,
  COLOR_SPACE_HSV,
  COLOR_SPACE_NONE
} ColorSpaceId;

typedef struct _Component
{
  GtkAdjustment *adj;
  gulong         handler;
  gdouble        val;
  gdouble        factor;
  ColorSpaceId   color_space;
} Component;

typedef struct
{
  cairo_surface_t        *surface;

  GstyleCssProvider      *default_provider;

  GtkGesture             *drag_gesture;
  GtkGesture             *long_press_gesture;

  GtkBorder               cached_margin;
  GtkBorder               cached_border;
  GdkRectangle            cached_margin_box;
  GdkRectangle            cached_border_box;

  GstyleColorPlaneMode    mode;
  GstyleXYZ               xyz;
  gdouble                 cursor_x;
  gdouble                 cursor_y;

  ComputeData             data;
  GstyleColorFilterFunc   filter;
  gpointer                filter_user_data;

  Component               comp [N_GSTYLE_COLOR_COMPONENT];
  GstyleColorComponent    ref_comp;
  GstyleColorUnit         preferred_unit;
  gdouble                 hue_backup;

  guint                   hue_backup_set : 1;
} GstyleColorPlanePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GstyleColorPlane, gstyle_color_plane, GTK_TYPE_DRAWING_AREA)

enum {
  PROP_0,
  PROP_MODE,
  PROP_RGBA,
  PROP_XYZ,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/* We return %TRUE if there's no changes in border and margin, %FALSE otherwise.*/
static gboolean
update_css_boxes (GstyleColorPlane *self)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *style_context;
  GtkStateFlags state;
  GdkRectangle margin_box;
  GdkRectangle border_box;
  GtkBorder margin;
  GtkBorder border;
  gboolean res;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state = gtk_style_context_get_state (style_context);

  gtk_style_context_get_margin (style_context, state, &margin);
  gtk_style_context_get_border (style_context, state, &border);
  gtk_widget_get_allocation (widget, &margin_box);
  margin_box.x = margin_box.y = 0;

  gstyle_utils_get_rect_resized_box (margin_box, &margin_box, &margin);
  gstyle_utils_get_rect_resized_box (margin_box, &border_box, &border);

  res = (gstyle_utils_cmp_border (margin, priv->cached_margin) ||
         gstyle_utils_cmp_border (border, priv->cached_border));

  priv->cached_margin_box = margin_box;
  priv->cached_border_box = border_box;
  priv->cached_margin = margin;
  priv->cached_border = border;

  return res;
}

static void
get_xyz_from_cursor (GstyleColorPlane *self,
                     GstyleXYZ        *xyz)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hsv_h, hsv_s, hsv_v;
  GstyleCielab lab;
  GdkRGBA rgba = {0};

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (xyz != NULL);

  switch (priv->mode)
    {
    case GSTYLE_COLOR_PLANE_MODE_HUE:
      hsv_h = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].factor,
      hsv_s = priv->cursor_x * priv->data.x_factor;
      hsv_v = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_hsv_to_xyz (hsv_h, hsv_s, hsv_v, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_SATURATION:
      hsv_s = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].factor,
      hsv_h = priv->cursor_x * priv->data.x_factor;
      hsv_v = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_hsv_to_xyz (hsv_h, hsv_s, hsv_v, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS:
      hsv_v = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].factor,
      hsv_h = priv->cursor_x * priv->data.x_factor;
      hsv_s = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_hsv_to_xyz (hsv_h, hsv_s, hsv_v, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_L:
      lab.l = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].factor,
      lab.a = priv->cursor_x * priv->data.lab_x_factor - 128.0;
      lab.b = (priv->data.height - priv->cursor_y - 1) * priv->data.lab_y_factor - 128.0;
      gstyle_color_convert_cielab_to_xyz (&lab, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_A:
      lab.a = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].factor,
      lab.b = priv->cursor_x * priv->data.lab_x_factor - 128.0;
      lab.l = (priv->data.height - priv->cursor_y - 1) * priv->data.lab_l_factor;
      gstyle_color_convert_cielab_to_xyz (&lab, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_B:
      lab.b = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].factor,
      lab.a = priv->cursor_x * priv->data.lab_x_factor - 128.0;
      lab.l = (priv->data.height - priv->cursor_y - 1) * priv->data.lab_l_factor;
      gstyle_color_convert_cielab_to_xyz (&lab, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_RED:
      rgba.red = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].factor,
      rgba.blue = priv->cursor_x * priv->data.x_factor;
      rgba.green = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_rgb_to_xyz (&rgba, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_GREEN:
      rgba.green = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].factor,
      rgba.blue = priv->cursor_x * priv->data.x_factor;
      rgba.red = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_rgb_to_xyz (&rgba, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_BLUE:
      rgba.blue = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].factor,
      rgba.red = priv->cursor_x * priv->data.x_factor;
      rgba.green = (priv->data.height - priv->cursor_y - 1) * priv->data.y_factor;
      gstyle_color_convert_rgb_to_xyz (&rgba, xyz);
      break;

    case GSTYLE_COLOR_PLANE_MODE_NONE:
    default:
      g_assert_not_reached ();
    }
}

static void
set_cursor_from_xyz (GstyleColorPlane *self,
                     GstyleXYZ        *xyz)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hsv_h, hsv_s, hsv_v;
  GstyleCielab lab;
  GdkRGBA rgba = {0};
  gdouble x = 0.0, y = 0.0;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (xyz != NULL);

  switch (priv->mode)
    {
    case GSTYLE_COLOR_PLANE_MODE_HUE:
      gstyle_color_convert_xyz_to_hsv (xyz, &hsv_h, &hsv_s, &hsv_v);
      x = hsv_s / priv->data.x_factor;
      y = (1.0 - hsv_v) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_SATURATION:
      gstyle_color_convert_xyz_to_hsv (xyz, &hsv_h, &hsv_s, &hsv_v);
      x = hsv_h / priv->data.x_factor;
      y = (1.0 - hsv_v) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS:
      gstyle_color_convert_xyz_to_hsv (xyz, &hsv_h, &hsv_s, &hsv_v);
      x = hsv_h / priv->data.x_factor;
      y = (1.0 - hsv_s) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_L:
      gstyle_color_convert_xyz_to_cielab (xyz, &lab);
      x = (lab.a + 128.0) / priv->data.lab_x_factor;
      y = (128.0 - lab.b) / priv->data.lab_y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_A:
      gstyle_color_convert_xyz_to_cielab (xyz, &lab);
      x = (lab.b + 128.0) / priv->data.lab_x_factor;
      y = (100.0 - lab.l) / priv->data.lab_l_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_B:
      gstyle_color_convert_xyz_to_cielab (xyz, &lab);
      x = (lab.a + 128.0) / priv->data.lab_x_factor;
      y = (100.0 - lab.l) / priv->data.lab_y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_RED:
      gstyle_color_convert_xyz_to_rgb (xyz, &rgba);
      x = rgba.blue / priv->data.x_factor;
      y = (1.0 - rgba.green) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_GREEN:
      gstyle_color_convert_xyz_to_rgb (xyz, &rgba);
      x = rgba.blue / priv->data.x_factor;
      y = (1.0 - rgba.red) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_BLUE:
      gstyle_color_convert_xyz_to_rgb (xyz, &rgba);
      x = rgba.red / priv->data.x_factor;
      y = (1.0 - rgba.green) / priv->data.y_factor;
      break;

    case GSTYLE_COLOR_PLANE_MODE_NONE:
    default:
      g_assert_not_reached ();
    }

  priv->cursor_x = CLAMP (x, 0.0, (gdouble)priv->data.width - 1.0);
  priv->cursor_y = CLAMP (y, 0.0, (gdouble)priv->data.height - 1.0);
}

static void
configure_component (GstyleColorPlane     *self,
                     GstyleColorComponent  comp,
                     gdouble               upper,
                     gdouble               factor)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble new_value;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (GTK_IS_ADJUSTMENT (priv->comp [comp].adj));

  new_value = priv->comp [comp].val / priv->comp [comp].factor * factor;
  priv->comp [comp].factor = factor;

  g_object_freeze_notify (G_OBJECT (priv->comp [comp].adj));
  gtk_adjustment_set_upper (priv->comp [comp].adj, upper);
  gtk_adjustment_set_value (priv->comp [comp].adj, new_value);
  g_object_thaw_notify (G_OBJECT (priv->comp [comp].adj));
}

static void
setup_component (GstyleColorPlane     *self,
                 GstyleColorComponent  comp,
                 gdouble               origin,
                 gdouble               lower,
                 gdouble               upper,
                 gdouble               step_increment,
                 gdouble               page_increment,
                 gdouble               factor,
                 ColorSpaceId          color_space)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  priv->comp [comp].adj = g_object_ref (gtk_adjustment_new (origin, lower, upper, step_increment, page_increment, 0));
  priv->comp [comp].factor = factor;
  priv->comp [comp].color_space = color_space;
}

/**
 * gstyle_color_plane_set_preferred_unit:
 * @self: a #GstyleColorPlane
 * @preferred_unit: a #GstyleColorUnit enum value
 *
 * Set percent or value  as the preferred unit for rgb adjustment range.
 * [0, 100] for percent unit or [0, 255] for value.
 *
 */
void
gstyle_color_plane_set_preferred_unit (GstyleColorPlane *self,
                                       GstyleColorUnit   preferred_unit)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble max_range = 0.0;

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));

  if (priv->preferred_unit != preferred_unit)
    {
      priv->preferred_unit = preferred_unit;
      if (preferred_unit == GSTYLE_COLOR_UNIT_PERCENT)
        max_range = 100.0;
      else if (preferred_unit == GSTYLE_COLOR_UNIT_VALUE)
        max_range = 255.0;
      else
        g_assert_not_reached ();

      configure_component (self, GSTYLE_COLOR_COMPONENT_RGB_RED, max_range, max_range);
      configure_component (self, GSTYLE_COLOR_COMPONENT_RGB_GREEN, max_range, max_range);
      configure_component (self, GSTYLE_COLOR_COMPONENT_RGB_BLUE, max_range, max_range);
    }
}

/**
 * gstyle_color_plane_get_filter_func: (skip):
 * @self: a #GstyleColorPlane
 *
 * Get a pointer to the current filter function or %NULL
 * if no filter is actually set.
 *
 * Returns: (nullable): A GstyleColorFilterFunc function pointer.
 *
 */
GstyleColorFilterFunc
gstyle_color_plane_get_filter_func (GstyleColorPlane *self)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_val_if_fail (GSTYLE_IS_COLOR_PLANE (self), NULL);

  return priv->filter;
}

/**
 * gstyle_color_plane_set_filter_func:
 * @self: a #GstyleColorPlane
 * @filter_cb: (scope notified) (nullable): A GstyleColorFilterFunc filter function or
 *   %NULL to unset the current filter. In this case, user_data is ignored
 * @user_data: (closure) (nullable): user data to pass when calling the filter function
 *
 * Set a filter to be used to change the drawing of the color plane.
 *
 */
void
gstyle_color_plane_set_filter_func (GstyleColorPlane      *self,
                                    GstyleColorFilterFunc  filter_cb,
                                    gpointer               user_data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));

  priv->filter = filter_cb;
  priv->filter_user_data = (filter_cb == NULL) ? NULL : user_data;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
compute_plane_hue_mode (GstyleColorPlane *self,
                        ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hue, saturation, value;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  hue = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      value = CLAMP ((data.height - y) * data.y_factor, 0.0, 1.0);
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          saturation = x * data.x_factor;
          gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_saturation_mode (GstyleColorPlane *self,
                               ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hue, saturation, value;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  saturation = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      value = CLAMP ((data.height - y) * data.y_factor, 0.0, 1.0);
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          hue = x * data.x_factor;
          gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_brightness_mode (GstyleColorPlane *self,
                               ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hue, saturation, value;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  value = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].factor;
  for (gint y = 0; y < data.height - 1; ++y)
    {
      saturation = CLAMP ((data.height - y) * data.y_factor, 0.0, 1.0);
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          hue = x * data.x_factor;
          gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_cielab_l_mode (GstyleColorPlane *self,
                             ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GstyleCielab lab;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  lab.l = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      lab.b = (data.height - y) * data.lab_y_factor - 128.0;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          lab.a = x * data.lab_x_factor - 128.0;
          gstyle_color_convert_cielab_to_rgb (&lab, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_cielab_a_mode (GstyleColorPlane *self,
                             ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GstyleCielab lab;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  lab.a = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      lab.l = (data.height - y) * data.lab_l_factor;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          lab.b = x * data.lab_x_factor - 128.0;
          gstyle_color_convert_cielab_to_rgb (&lab, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_cielab_b_mode (GstyleColorPlane *self,
                             ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GstyleCielab lab;
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  lab.b = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      lab.l = (data.height - y) * data.lab_l_factor;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          lab.a = x * data.lab_x_factor - 128.0;
          gstyle_color_convert_cielab_to_rgb (&lab, &rgba);
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_red_mode (GstyleColorPlane *self,
                        ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  rgba.red = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      rgba.green = (data.height - y) * data.y_factor;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          rgba.blue = x * data.x_factor;
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_green_mode (GstyleColorPlane *self,
                          ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  rgba.green = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      rgba.red = (data.height - y) * data.y_factor;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          rgba.blue = x * data.x_factor;
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static void
compute_plane_blue_mode (GstyleColorPlane *self,
                         ComputeData       data)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GdkRGBA rgba = {0};
  guint32 *p;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  rgba.blue = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].factor;
  for (gint y = 0; y < data.height; ++y)
    {
      rgba.green = (data.height - y) * data.y_factor;
      p = data.buffer + y * (data.stride / 4);
      for (gint x = 0; x < data.width; ++x)
        {
          rgba.red = x * data.x_factor;
          if (priv->filter != NULL)
            priv->filter (&rgba, &rgba, priv->filter_user_data);

          p[x] = pack_rgba24 (&rgba);
        }
    }
}

static gboolean
create_surface (GstyleColorPlane *self)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GtkWidget *widget = (GtkWidget *)self;
  cairo_surface_t *surface;
  cairo_surface_t *tmp;
  cairo_t *cr;
  gint adjusted_height;
  gint adjusted_width;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  if (!gtk_widget_get_realized (widget))
    return FALSE;

  /* TODO: keep only one of priv->data.width or priv->cached_border_box.width */

  priv->data.width = priv->cached_border_box.width;
  priv->data.height = priv->cached_border_box.height;
  adjusted_height = priv->data.height - 1;
  adjusted_width = priv->data.width - 1;

  priv->data.y_factor = 1.0 / adjusted_height;
  priv->data.x_factor = 1.0 / adjusted_width;
  priv->data.lab_y_factor = 255.0 / adjusted_height;
  priv->data.lab_x_factor = 255.0 / adjusted_width;
  priv->data.lab_l_factor = 100.0 / adjusted_height;

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               priv->data.width, priv->data.height);

  if (priv->surface)
    cairo_surface_destroy (priv->surface);

  priv->surface = surface;

  if (priv->data.width <= 1 || priv->data.height <= 1)
    return FALSE;

  priv->data.stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, priv->data.width);
  priv->data.buffer = g_malloc (priv->data.height * priv->data.stride);

  switch (priv->mode)
    {
    case GSTYLE_COLOR_PLANE_MODE_HUE:
      compute_plane_hue_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_SATURATION:
      compute_plane_saturation_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS:
      compute_plane_brightness_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_L:
      compute_plane_cielab_l_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_A:
      compute_plane_cielab_a_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_CIELAB_B:
      compute_plane_cielab_b_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_RED:
      compute_plane_red_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_GREEN:
      compute_plane_green_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_BLUE:
      compute_plane_blue_mode (self, priv->data);
      break;

    case GSTYLE_COLOR_PLANE_MODE_NONE:
    default:
      g_assert_not_reached ();
    }

  tmp = cairo_image_surface_create_for_data ((guchar *)priv->data.buffer, CAIRO_FORMAT_RGB24,
                                             priv->data.width, priv->data.height, priv->data.stride);
  cr = cairo_create (surface);
  cairo_set_source_surface (cr, tmp, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (tmp);
  g_free (priv->data.buffer);

  return TRUE;
}

static gboolean
gstyle_color_plane_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
  GstyleColorPlane *self = (GstyleColorPlane *)widget;
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gint left_spacing;
  gint top_spacing;
  gint x, y;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (cr != NULL);

  if (!gtk_widget_get_visible (widget))
    return GDK_EVENT_PROPAGATE;

  if (update_css_boxes (self) || priv->surface == NULL)
    create_surface (self);

  left_spacing = priv->cached_margin.left + priv->cached_border.left;
  top_spacing = priv->cached_margin.top + priv->cached_border.top;
  x = round (priv->cursor_x) + left_spacing;
  y = round (priv->cursor_y) + top_spacing;

  cairo_set_source_surface (cr, priv->surface, priv->cached_border_box.x, priv->cached_border_box.y);
  cairo_paint (cr);

  gtk_render_frame (gtk_widget_get_style_context (widget), cr,
                    priv->cached_margin_box.x, priv->cached_margin_box.y,
                    priv->cached_margin_box.width, priv->cached_margin_box.height);

  cairo_move_to (cr, left_spacing, y + 0.5);
  cairo_line_to (cr, left_spacing + priv->cached_border_box.width, y + 0.5);

  cairo_move_to (cr, x + 0.5, top_spacing);
  cairo_line_to (cr, x + 0.5, top_spacing + priv->cached_border_box.height);

  if (gtk_widget_has_visible_focus (widget))
    {
      cairo_set_line_width (cr, 3.0);
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.6);
      cairo_stroke_preserve (cr);

      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.8);
      cairo_stroke (cr);
    }
  else
    {
      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 0.8);
      cairo_stroke (cr);
    }

  return FALSE;
}

static inline gboolean
compare_xyz (GstyleXYZ xyz1,
             GstyleXYZ xyz2)
{
  return (xyz1.x == xyz2.x &&
          xyz1.y == xyz2.y &&
          xyz1.z == xyz2.z &&
          xyz1.alpha == xyz2.alpha);
}

/* Adjustments are updated from the stored xyz */
static void
update_adjustments (GstyleColorPlane     *self,
                    GstyleXYZ            *xyz,
                    GstyleColorComponent  changed_comp)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hue, saturation, value;
  gdouble current_hue;
  GstyleCielab lab;
  ColorSpaceId color_space;
  GdkRGBA rgba = {0};

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (xyz != NULL);

  if (!compare_xyz (priv->xyz, *xyz))
    {
      color_space = (changed_comp == GSTYLE_COLOR_COMPONENT_NONE) ? COLOR_SPACE_NONE : priv->comp [changed_comp].color_space;
      if (color_space != COLOR_SPACE_RGB)
        {
          gstyle_color_convert_xyz_to_rgb (xyz, &rgba);
          priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].val = rgba.red * priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].factor;
          priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].val = rgba.green * priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].factor;
          priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].val = rgba.blue * priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].factor;
        }

      if (color_space != COLOR_SPACE_CIELAB)
        {
          gstyle_color_convert_xyz_to_cielab (xyz, &lab);
          priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].val = lab.l * priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].factor;
          priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].val = lab.a * priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].factor;
          priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].val = lab.b * priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].factor;
        }

      if (color_space != COLOR_SPACE_HSV)
        {
          current_hue = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val;
          gstyle_color_convert_xyz_to_hsv (xyz, &hue, &saturation, &value);
          if (saturation > 1e-6)
            {
              if (priv->hue_backup_set)
                {
                  priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val = priv->hue_backup;
                  priv->hue_backup_set = FALSE;
                }
              else
                priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val = hue * priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].factor;
            }
          else if (!priv->hue_backup_set)
            {
              priv->hue_backup = current_hue;
              priv->hue_backup_set = TRUE;
              priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val = hue;
            }

          priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].val = saturation * priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].factor;
          priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].val = value * priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].factor;
        }

      for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
        if (priv->comp [i].color_space != color_space)
          {
            g_signal_handler_block (priv->comp [i].adj, priv->comp [i].handler);
            gtk_adjustment_set_value (priv->comp [i].adj, priv->comp [i].val);
            g_signal_handler_unblock (priv->comp [i].adj, priv->comp [i].handler);
          }
    }
}

static void
update_surface_and_cursor (GstyleColorPlane *self,
                           gboolean          update_surface)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  if (update_surface)
    create_surface (self);

  set_cursor_from_xyz (self, &priv->xyz);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
update_cursor (GstyleColorPlane *self,
               gdouble           x,
               gdouble           y)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gint left_spacing;
  gint top_spacing;
  GstyleXYZ xyz = {0};

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  left_spacing = priv->cached_margin.left + priv->cached_border.left;
  top_spacing = priv->cached_margin.top + priv->cached_border.top;
  x = CLAMP (x - left_spacing, 0.0, priv->data.width - 1.0);
  y = CLAMP (y - top_spacing, 0.0, priv->data.height - 1.0);

  if (priv->cursor_x != x || priv->cursor_y != y)
    {
      priv->cursor_x = x;
      priv->cursor_y = y;

      get_xyz_from_cursor (self, &xyz);
      update_adjustments (self, &xyz, GSTYLE_COLOR_COMPONENT_NONE);
      priv->xyz = xyz;

      gtk_widget_queue_draw (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RGBA]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_XYZ]);
    }
}

static void
move_cursor (GstyleColorPlane *self,
             gdouble           step_x,
             gdouble           step_y)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  update_cursor (self, priv->cursor_x + step_x, priv->cursor_y - step_y);
  /* TODO: ring when reaching the border */
}

static GstyleColorComponent
get_adj_id (GstyleColorPlane *self,
            GtkAdjustment    *adj)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (GTK_IS_ADJUSTMENT (adj));

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    if (adj == priv->comp [i].adj)
      return i;

  g_return_val_if_reached (0);
}

static void
adjustments_changed (GstyleColorPlane *self,
                     GtkAdjustment    *adj)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hue, saturation, value;
  GdkRGBA rgba;
  GstyleXYZ xyz;
  GstyleCielab lab;
  GstyleColorComponent changed_comp;
  gdouble old_ref_val;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));
  g_assert (GTK_IS_ADJUSTMENT (adj));

  old_ref_val = priv->comp [priv->ref_comp].val;
  changed_comp = get_adj_id (self, adj);
  priv->comp [changed_comp].val = gtk_adjustment_get_value (priv->comp [changed_comp].adj);

  if (changed_comp == GSTYLE_COLOR_COMPONENT_HSV_H ||
      changed_comp == GSTYLE_COLOR_COMPONENT_HSV_S ||
      changed_comp == GSTYLE_COLOR_COMPONENT_HSV_V)
    {
      hue = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_H].factor;
      saturation = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_S].factor;
      value = priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].val / priv->comp [GSTYLE_COLOR_COMPONENT_HSV_V].factor;

      gstyle_color_convert_hsv_to_xyz (hue, saturation, value, &xyz);
    }
  else if (changed_comp == GSTYLE_COLOR_COMPONENT_LAB_L ||
           changed_comp == GSTYLE_COLOR_COMPONENT_LAB_A ||
           changed_comp == GSTYLE_COLOR_COMPONENT_LAB_B)
    {
      lab.l = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_L].factor;
      lab.a = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_A].factor;
      lab.b = priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].val / priv->comp [GSTYLE_COLOR_COMPONENT_LAB_B].factor;

      gstyle_color_convert_cielab_to_xyz (&lab, &xyz);
    }
  else
    {
      rgba.red = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_RED].factor;
      rgba.green = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_GREEN].factor;
      rgba.blue = priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].val / priv->comp [GSTYLE_COLOR_COMPONENT_RGB_BLUE].factor;
      gstyle_color_convert_rgb_to_xyz (&rgba, &xyz);
    }

  xyz.alpha = 1;
  update_adjustments (self, &xyz, changed_comp);
  priv->xyz = xyz;
  update_surface_and_cursor (self, old_ref_val != priv->comp [priv->ref_comp].val);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_XYZ]);
}

/**
 * gstyle_color_plane_new:
 *
 * Returns: a new #GstyleColorPlane
 */
GstyleColorPlane *
gstyle_color_plane_new (void)
{
  return g_object_new (GSTYLE_TYPE_COLOR_PLANE, NULL);
}

static void
gstyle_color_plane_size_allocate (GtkWidget     *widget,
                                  GtkAllocation *allocation)
{
  GstyleColorPlane *self = GSTYLE_COLOR_PLANE (widget);
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  GTK_WIDGET_CLASS (gstyle_color_plane_parent_class)->size_allocate (widget, allocation);

  update_css_boxes (self);

  if (create_surface (GSTYLE_COLOR_PLANE (widget)))
    set_cursor_from_xyz (self, &priv->xyz);
}

static gboolean
gstyle_color_plane_key_press (GtkWidget   *widget,
                              GdkEventKey *event)
{
  GstyleColorPlane *self = GSTYLE_COLOR_PLANE (widget);
  gdouble step;

  g_assert (event != NULL);

  if ((event->state & GDK_MOD1_MASK) != 0)
    step = 0.1;
  else
    step = 0.01;

  if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up)
    move_cursor (self, 0, step);
  else if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
    move_cursor (self, 0, -step);
  else if (event->keyval == GDK_KEY_Left || event->keyval == GDK_KEY_KP_Left)
    move_cursor (self, -step, 0);
  else if (event->keyval == GDK_KEY_Right ||  event->keyval == GDK_KEY_KP_Right)
    move_cursor (self, step, 0);
  else
    return GTK_WIDGET_CLASS (gstyle_color_plane_parent_class)->key_press_event (widget, event);

  return TRUE;
}

/**
 * gstyle_color_plane_get_xyz:
 * @self: a #GstyleColorPlane
 * @xyz: (out): a #GstyleXYZ adress
 *
 * Fill @xyz with value at cursor position.
 * The alpha component is always equal to 1.
 *
 */
void
gstyle_color_plane_get_xyz (GstyleColorPlane *self,
                            GstyleXYZ        *xyz)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));
  g_return_if_fail (xyz != NULL);

  *xyz = priv->xyz;
}

/**
 * gstyle_color_plane_get_rgba:
 * @self: a #GstyleColorPlane
 * @rgba: (out): a #GdkRGBA adress
 *
 * Fill @rgba with value at cursor position.
 * The alpha component is always equal to 1.
 *
 */
void
gstyle_color_plane_get_rgba (GstyleColorPlane *self,
                             GdkRGBA          *rgba)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));
  g_return_if_fail (rgba != NULL);

  gstyle_color_convert_xyz_to_rgb (&priv->xyz, rgba);
}

/**
 * gstyle_color_plane_get_filtered_rgba:
 * @self: a #GstyleColorPlane
 * @rgba: (out): a #GdkRGBA adress
 *
 * Fill @rgba with filtered value at cursor position.
 *
 */
void
gstyle_color_plane_get_filtered_rgba (GstyleColorPlane *self,
                                      GdkRGBA          *rgba)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));
  g_return_if_fail (rgba != NULL);

  gstyle_color_convert_xyz_to_rgb (&priv->xyz, rgba);
  priv->filter (rgba, rgba, priv->filter_user_data);
}

/**
 * gstyle_color_plane_get_component_adjustment:
 * @self: a #GstyleColorPlane
 * @comp: a #GstyleColorComponent enum value
 *
 * Return the color component adjustment designated by
 * the #GstyleColorComponent value.
 *
 * Returns: (transfer none): #GtkAdjustment.
 *
 */
GtkAdjustment *
gstyle_color_plane_get_component_adjustment (GstyleColorPlane     *self,
                                             GstyleColorComponent  comp)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_val_if_fail (GSTYLE_IS_COLOR_PLANE (self), NULL);
  g_return_val_if_fail (comp !=  GSTYLE_COLOR_COMPONENT_NONE, NULL);

  return priv->comp [comp].adj;
}

/**
 * gstyle_color_plane_set_rgba:
 * @self: a #GstyleColorPlane
 * @rgba: a #GdkRGBA
 *
 * Set cursor position from @rgba value.
 *
 */
void
gstyle_color_plane_set_rgba (GstyleColorPlane *self,
                             const GdkRGBA    *rgba)
{
  GstyleColorPlanePrivate *priv;
  GstyleXYZ xyz = {0};

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));
  g_return_if_fail (rgba != NULL);

  priv = gstyle_color_plane_get_instance_private (self);

  gstyle_color_convert_rgb_to_xyz ((GdkRGBA *)rgba, &xyz);
  if (compare_xyz (xyz, priv->xyz))
    return;

  update_adjustments (self, &xyz, GSTYLE_COLOR_COMPONENT_NONE);
  priv->xyz = xyz;
  update_surface_and_cursor (self, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_XYZ]);
}

/**
 * gstyle_color_plane_set_xyz:
 * @self: a #GstyleColorPlane.
 * @xyz: a #GstyleXYZ struct.
 *
 * Set cursor position from @rgba value.
 *
 */
void
gstyle_color_plane_set_xyz (GstyleColorPlane *self,
                            const GstyleXYZ  *xyz)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));
  g_return_if_fail (xyz != NULL);

  if (compare_xyz (*xyz, priv->xyz))
    return;

  update_adjustments (self, (GstyleXYZ *)xyz, GSTYLE_COLOR_COMPONENT_NONE);
  priv->xyz = *xyz;
  update_surface_and_cursor (self, TRUE);

  /* TODO: add xyz props */
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_XYZ]);
}

/**
 * gstyle_color_plane_set_mode:
 * @self: a #GstyleColorPlane.
 * @mode: a #GstylewColorPlaneMode.
 *
 * Set the displayed mode to use.
 *
 */
void
gstyle_color_plane_set_mode (GstyleColorPlane     *self,
                             GstyleColorPlaneMode  mode)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  gdouble hsv_h, hsv_s, hsv_v;
  gdouble ref_val = 0.0;
  GstyleCielab lab;
  GdkRGBA rgba = {0};

  g_return_if_fail (GSTYLE_IS_COLOR_PLANE (self));

  if (priv->mode != mode)
    {
      priv->mode = mode;

      switch (priv->mode)
      {
      case GSTYLE_COLOR_PLANE_MODE_HUE:
        gstyle_color_convert_xyz_to_hsv (&priv->xyz, &hsv_h, &hsv_s, &hsv_v);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_HSV_H;
        ref_val = hsv_h;
        break;

      case GSTYLE_COLOR_PLANE_MODE_SATURATION:
        gstyle_color_convert_xyz_to_hsv (&priv->xyz, &hsv_h, &hsv_s, &hsv_v);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_HSV_S;
        ref_val = hsv_s;
        break;

      case GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS:
        gstyle_color_convert_xyz_to_hsv (&priv->xyz, &hsv_h, &hsv_s, &hsv_v);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_HSV_V;
        ref_val = hsv_v;
        break;

      case GSTYLE_COLOR_PLANE_MODE_CIELAB_L:
        gstyle_color_convert_xyz_to_cielab (&priv->xyz, &lab);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_LAB_L;
        ref_val = lab.l;
        break;

      case GSTYLE_COLOR_PLANE_MODE_CIELAB_A:
        gstyle_color_convert_xyz_to_cielab (&priv->xyz, &lab);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_LAB_A;
        ref_val = lab.a;
        break;

      case GSTYLE_COLOR_PLANE_MODE_CIELAB_B:
        gstyle_color_convert_xyz_to_cielab (&priv->xyz, &lab);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_LAB_B;
        ref_val = lab.b;
        break;

      case GSTYLE_COLOR_PLANE_MODE_RED:
        gstyle_color_convert_xyz_to_rgb (&priv->xyz, &rgba);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_RGB_RED;
        ref_val = rgba.red;
        break;

      case GSTYLE_COLOR_PLANE_MODE_GREEN:
        gstyle_color_convert_xyz_to_rgb (&priv->xyz, &rgba);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_RGB_GREEN;
        ref_val = rgba.green;
        break;

      case GSTYLE_COLOR_PLANE_MODE_BLUE:
        gstyle_color_convert_xyz_to_rgb (&priv->xyz, &rgba);
        priv->ref_comp = GSTYLE_COLOR_COMPONENT_RGB_BLUE;
        ref_val = rgba.blue;
        break;

      case GSTYLE_COLOR_PLANE_MODE_NONE:
      default:
        g_assert_not_reached ();
      }

      g_signal_handler_block (priv->comp [priv->ref_comp].adj, priv->comp [priv->ref_comp].handler);

      priv->comp [priv->ref_comp].val = ref_val * priv->comp [priv->ref_comp].factor;
      gtk_adjustment_set_value (priv->comp [priv->ref_comp].adj, priv->comp [priv->ref_comp].val);

      g_signal_handler_unblock (priv->comp [priv->ref_comp].adj, priv->comp [priv->ref_comp].handler);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODE]);
      update_surface_and_cursor (self, TRUE);
    }
}

static void
gstyle_color_plane_destroy (GtkWidget *widget)
{
  GstyleColorPlane *self = (GstyleColorPlane *)widget;
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  gstyle_clear_pointer (&priv->surface, cairo_surface_destroy);

  GTK_WIDGET_CLASS (gstyle_color_plane_parent_class)->destroy (widget);
}

static void
gstyle_color_plane_finalize (GObject *object)
{
  GstyleColorPlane *self = (GstyleColorPlane *)object;
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);

  g_clear_object (&priv->drag_gesture);
  g_clear_object (&priv->long_press_gesture);
  g_clear_object (&priv->default_provider);

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    g_clear_object (&priv->comp [i].adj);

  G_OBJECT_CLASS (gstyle_color_plane_parent_class)->finalize (object);
}

static void
gstyle_color_plane_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GstyleColorPlane *self = GSTYLE_COLOR_PLANE (object);
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GdkRGBA rgba = {0};
  GstyleXYZ xyz;

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;

    case PROP_RGBA:
      gstyle_color_plane_get_rgba (self, &rgba);
      g_value_set_boxed (value, &rgba);
      break;

    case PROP_XYZ:
      gstyle_color_plane_get_xyz (self, &xyz);
      g_value_set_boxed (value, &xyz);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_plane_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GstyleColorPlane *self = GSTYLE_COLOR_PLANE (object);
  GdkRGBA *rgba_p;
  GstyleXYZ *xyz_p;
  GstyleXYZ xyz = {0};
  GdkRGBA rgba = {0.5, 0.3, 0.3, 0.0};

  switch (prop_id)
    {
    case PROP_MODE:
      gstyle_color_plane_set_mode (self, g_value_get_enum (value));
      break;

    case PROP_RGBA:
      rgba_p = (GdkRGBA *)g_value_get_boxed (value);
      if (rgba_p == NULL)
        rgba_p = &rgba;

      gstyle_color_plane_set_rgba (self, rgba_p);
      break;

    case PROP_XYZ:
      xyz_p = (GstyleXYZ *)g_value_get_boxed (value);
      if (xyz_p == NULL)
        {
          gstyle_color_convert_rgb_to_xyz (&rgba, &xyz);
          gstyle_color_plane_set_xyz (self, &xyz);
        }
      else
        gstyle_color_plane_set_xyz (self, xyz_p);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_plane_class_init (GstyleColorPlaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gstyle_color_plane_finalize;
  object_class->get_property = gstyle_color_plane_get_property;
  object_class->set_property = gstyle_color_plane_set_property;

  widget_class->draw = gstyle_color_plane_draw;
  widget_class->size_allocate = gstyle_color_plane_size_allocate;
  widget_class->key_press_event = gstyle_color_plane_key_press;
  widget_class->destroy = gstyle_color_plane_destroy;

  properties [PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Mode",
                       "The mode displayed",
                       GSTYLE_TYPE_COLOR_PLANE_MODE,
                       GSTYLE_COLOR_PLANE_MODE_HUE,
                       (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RGBA] =
    g_param_spec_boxed ("rgba",
                        "rgba",
                        "Color pointed by the cursor",
                        GDK_TYPE_RGBA,
                        (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_XYZ] =
    g_param_spec_boxed ("xyz",
                        "xyz",
                        "Color pointed by the cursor",
                        GSTYLE_TYPE_XYZ,
                        (G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* TODO: add *_adjustment properties */

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gstylecolorplane");
}

static void
set_cross_cursor (GtkWidget *widget,
                  gboolean   enabled)
{
  GstyleColorPlane *self = (GstyleColorPlane *)widget;
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GdkCursor *cursor = NULL;
  GdkWindow *window;
  GdkDevice *device;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  window = gtk_widget_get_window (widget);
  device = gtk_gesture_get_device (priv->drag_gesture);

  if (!window || !device)
    return;

  if (enabled)
    cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (widget)), "crosshair");

  gdk_window_set_device_cursor (window, device, cursor);

  if (cursor)
    g_object_unref (cursor);
}

static void
hold_action (GtkGestureLongPress *gesture,
             gdouble              x,
             gdouble              y,
             GstyleColorPlane    *self)
{
  gboolean handled;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  g_signal_emit_by_name (self, "popup-menu", &handled);
}

static void
drag_gesture_begin (GtkGestureDrag   *gesture,
                    gdouble           start_x,
                    gdouble           start_y,
                    GstyleColorPlane *self)
{
  /* GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self); */
  guint button;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  if (button == GDK_BUTTON_SECONDARY)
    {
      gboolean handled;
      g_signal_emit_by_name (self, "popup-menu", &handled);
    }

  if (button != GDK_BUTTON_PRIMARY)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  set_cross_cursor (GTK_WIDGET (self), TRUE);
  update_cursor (self, start_x, start_y);

  gtk_widget_grab_focus (GTK_WIDGET (self));
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
drag_gesture_update (GtkGestureDrag   *gesture,
                     gdouble           offset_x,
                     gdouble           offset_y,
                     GstyleColorPlane *self)
{
  /* GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self); */
  gdouble start_x, start_y;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (gesture),
                                    &start_x, &start_y);

  update_cursor (self, start_x + offset_x, start_y + offset_y);
}

static void
drag_gesture_end (GtkGestureDrag   *gesture,
                  gdouble           offset_x,
                  gdouble           offset_y,
                  GstyleColorPlane *self)
{
  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  set_cross_cursor (GTK_WIDGET (self), FALSE);
}

static void
gstyle_color_plane_init (GstyleColorPlane *self)
{
  GstyleColorPlanePrivate *priv = gstyle_color_plane_get_instance_private (self);
  GtkStyleContext *context;
  AtkObject *atk_obj;

  g_assert (GSTYLE_IS_COLOR_PLANE (self));

  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
  gtk_widget_set_events (GTK_WIDGET (self), GDK_KEY_PRESS_MASK |
                                            GDK_TOUCH_MASK |
                                            GDK_BUTTON_PRESS_MASK |
                                            GDK_BUTTON_RELEASE_MASK |
                                            GDK_POINTER_MOTION_MASK);

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (self));
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      atk_object_set_name (atk_obj, _("Color Plane"));
      atk_object_set_role (atk_obj, ATK_ROLE_COLOR_CHOOSER);
    }

  setup_component (self, GSTYLE_COLOR_COMPONENT_HSV_H, 0.0, 0.0, 360.0, 1.0, 1.0, 360.0, COLOR_SPACE_HSV);
  setup_component (self, GSTYLE_COLOR_COMPONENT_HSV_S, 0.0, 0.0, 100.0, 1.0, 1.0, 100.0, COLOR_SPACE_HSV);
  setup_component (self, GSTYLE_COLOR_COMPONENT_HSV_V, 0.0, 0.0, 100.0, 1.0, 1.0, 100.0, COLOR_SPACE_HSV);

  setup_component (self, GSTYLE_COLOR_COMPONENT_LAB_L, 0.0, 0.0, 100.0, 1.0, 1.0, 1.0, COLOR_SPACE_CIELAB);
  setup_component (self, GSTYLE_COLOR_COMPONENT_LAB_A, 0.0, -128.0, 128.0, 1.0, 1.0, 1.0, COLOR_SPACE_CIELAB);
  setup_component (self, GSTYLE_COLOR_COMPONENT_LAB_B, 0.0, -128.0, 128.0, 1.0, 1.0, 1.0, COLOR_SPACE_CIELAB);

  setup_component (self, GSTYLE_COLOR_COMPONENT_RGB_RED, 0.0, 0.0, 255.0, 1.0, 1.0, 255.0, COLOR_SPACE_RGB);
  setup_component (self, GSTYLE_COLOR_COMPONENT_RGB_GREEN, 0.0, 0.0, 255.0, 1.0, 1.0, 255.0, COLOR_SPACE_RGB);
  setup_component (self, GSTYLE_COLOR_COMPONENT_RGB_BLUE, 0.0, 0.0, 255.0, 1.0, 1.0, 255.0, COLOR_SPACE_RGB);

  priv->preferred_unit = GSTYLE_COLOR_UNIT_VALUE;

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    priv->comp [i].handler = g_signal_connect_swapped (priv->comp [i].adj,
                                                       "value-changed",
                                                       G_CALLBACK (adjustments_changed), self);

  priv->drag_gesture = gtk_gesture_drag_new (GTK_WIDGET (self));
  g_signal_connect (priv->drag_gesture, "drag-begin", G_CALLBACK (drag_gesture_begin), self);
  g_signal_connect (priv->drag_gesture, "drag-update", G_CALLBACK (drag_gesture_update), self);
  g_signal_connect (priv->drag_gesture, "drag-end", G_CALLBACK (drag_gesture_end), self);

  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (priv->drag_gesture), 0);

  priv->long_press_gesture = gtk_gesture_long_press_new (GTK_WIDGET (self));
  g_signal_connect (priv->long_press_gesture, "pressed", G_CALLBACK (hold_action), self);

  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (priv->long_press_gesture), TRUE);

  priv->mode = GSTYLE_COLOR_PLANE_MODE_HUE;
  priv->ref_comp = GSTYLE_COLOR_COMPONENT_HSV_H;
  priv->xyz.alpha = 1;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  priv->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));
}

GType
gstyle_color_plane_mode_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_PLANE_MODE_HUE, "GSTYLE_COLOR_PLANE_MODE_HUE", "hue" },
    { GSTYLE_COLOR_PLANE_MODE_SATURATION, "GSTYLE_COLOR_PLANE_MODE_SATURATION", "saturation" },
    { GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS, "GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS", "brightness" },
    { GSTYLE_COLOR_PLANE_MODE_CIELAB_L, "GSTYLE_COLOR_PLANE_MODE_CIELAB_L", "cielab-l" },
    { GSTYLE_COLOR_PLANE_MODE_CIELAB_A, "GSTYLE_COLOR_PLANE_MODE_CIELAB_A", "cielab-a" },
    { GSTYLE_COLOR_PLANE_MODE_CIELAB_B, "GSTYLE_COLOR_PLANE_MODE_CIELAB_B", "cielab-b" },
    { GSTYLE_COLOR_PLANE_MODE_RED, "GSTYLE_COLOR_PLANE_MODE_RED", "red" },
    { GSTYLE_COLOR_PLANE_MODE_GREEN, "GSTYLE_COLOR_PLANE_MODE_GREEN", "green" },
    { GSTYLE_COLOR_PLANE_MODE_BLUE, "GSTYLE_COLOR_PLANE_MODE_BLUE", "blue" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorPlaneMode", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
