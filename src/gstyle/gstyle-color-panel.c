/* gstyle-color-panel.c
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

#define G_LOG_DOMAIN "gstyle-color-panel"

#include <glib/gi18n.h>

#include "gstyle-resources.h"

#include "gstyle-color-panel-private.h"
#include "gstyle-color-panel-actions.h"
#include "gstyle-revealer.h"
#include "gstyle-color.h"

#define HSV_TO_SCALE_FACTOR (1.0 / 256.0)
#define CIELAB_L_TO_SCALE_FACTOR (100.0 / 256.0)

#define PREFS_BOX_MARGIN (10)

G_DEFINE_TYPE (GstyleColorPanel, gstyle_color_panel, GTK_TYPE_BOX)

static const gchar *comp_names [N_GSTYLE_COLOR_COMPONENT] = {
  "hsv_h",
  "hsv_s",
  "hsv_v",
  "lab_l",
  "lab_a",
  "lab_b",
  "rgb_red",
  "rgb_green",
  "rgb_blue"
};

/* Conversion between component and color plane mode :
 * component without a corresponding mode return
 * GSTYLE_COLOR_PLANE_MODE_NONE
 */
static GstyleColorPlaneMode component_to_plane_mode [N_GSTYLE_COLOR_COMPONENT] =
  {
    GSTYLE_COLOR_PLANE_MODE_HUE,
    GSTYLE_COLOR_PLANE_MODE_SATURATION,
    GSTYLE_COLOR_PLANE_MODE_BRIGHTNESS,
    GSTYLE_COLOR_PLANE_MODE_CIELAB_L,
    GSTYLE_COLOR_PLANE_MODE_CIELAB_A,
    GSTYLE_COLOR_PLANE_MODE_CIELAB_B,
    GSTYLE_COLOR_PLANE_MODE_RED,
    GSTYLE_COLOR_PLANE_MODE_GREEN,
    GSTYLE_COLOR_PLANE_MODE_BLUE
  };

enum {
  PROP_0,
  PROP_FILTER,
  PROP_HSV_VISIBLE,
  PROP_LAB_VISIBLE,
  PROP_RGB_VISIBLE,
  PROP_RGB_UNIT,
  PROP_STRINGS_VISIBLE,
  PROP_RGBA,
  PROP_XYZ,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  UPDATE_PREFS,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

/**
 * gstyle_color_panel_get_filter:
 * @self: a #GstyleColorPanel.
 *
 * Get the current color filter.
 *
 */
GstyleColorFilter
gstyle_color_panel_get_filter (GstyleColorPanel *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_PANEL (self), GSTYLE_COLOR_FILTER_NONE);

  return self->filter;
}

static void
update_color_strings (GstyleColorPanel *self,
                      GstyleColor      *color)
{
  gchar *str;
  g_autofree gchar *str_rgb = NULL;
  g_autofree gchar *str_rgba = NULL;
  const gchar *label;

  str = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_HEX3);
  label = gtk_label_get_label (self->res_hex3_label);
  if (g_strcmp0 (str, label) != 0)
    gtk_label_set_label (self->res_hex3_label, str);
  g_free (str);

  str = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_HEX6);
  label = gtk_label_get_label (self->res_hex6_label);
  if (g_strcmp0 (str, label) != 0)
    gtk_label_set_label (self->res_hex6_label, str);
  g_free (str);

  if (self->preferred_unit == GSTYLE_COLOR_UNIT_PERCENT)
    {
      str_rgb = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB_PERCENT);
      str_rgba = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGBA_PERCENT);
    }
  else if (self->preferred_unit == GSTYLE_COLOR_UNIT_VALUE)
    {
      str_rgb = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGB);
      str_rgba = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_RGBA);
    }
  else
    g_assert_not_reached ();

  label = gtk_label_get_label (self->res_rgb_label);
  if (g_strcmp0 (str_rgb, label) != 0)
    gtk_label_set_label (self->res_rgb_label, str_rgb);

  label = gtk_label_get_label (self->res_rgba_label);
  if (g_strcmp0 (str_rgba, label) != 0)
    gtk_label_set_label (self->res_rgba_label, str_rgba);

  str = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_HSL);
  label = gtk_label_get_label (self->res_hsl_label);
  if (g_strcmp0 (str, label) != 0)
    gtk_label_set_label (self->res_hsl_label, str);
  g_free (str);

  str = gstyle_color_to_string (color, GSTYLE_COLOR_KIND_HSLA);
  label = gtk_label_get_label (self->res_hsla_label);
  if (g_strcmp0 (str, label) != 0)
    gtk_label_set_label (self->res_hsla_label, str);
  g_free (str);
}

static void
adj_alpha_value_changed_cb (GstyleColorPanel *self,
                            GtkAdjustment    *adj)
{
  gdouble alpha;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  alpha = gtk_adjustment_get_value (self->adj_alpha) / 100.0;
  gstyle_color_set_alpha (self->old_color, alpha);
  update_color_strings (self, gstyle_color_widget_get_filtered_color (self->old_swatch));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_XYZ]);
}

/**
 * gstyle_color_panel_set_filter:
 * @self: a #GstyleColorPanel
 * @filter: a #GstyleColorFilter
 *
 * Set the color filter to use.
 *
 */
void
gstyle_color_panel_set_filter (GstyleColorPanel  *self,
                               GstyleColorFilter  filter)
{
  GstyleColorFilterFunc filter_func = NULL;

  g_return_if_fail (GSTYLE_IS_COLOR_PANEL (self));

  self->filter = filter;

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

  gstyle_color_widget_set_filter_func (self->new_swatch, filter_func, NULL);
  gstyle_color_widget_set_filter_func (self->old_swatch, filter_func, NULL);
  gstyle_color_plane_set_filter_func (self->color_plane, filter_func, NULL);
  gstyle_color_scale_set_filter_func (self->ref_scale, filter_func, NULL);
  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    gstyle_color_scale_set_filter_func (self->components [i].scale, filter_func, NULL);

  adj_alpha_value_changed_cb (self, self->adj_alpha);
}

/**
 * gstyle_color_panel_get_rgba:
 * @self: a #GstyleColorPanel.
 * @rgba: (out): a #GdkRGBA adress.
 *
 * Fill @rgba with the current color plane rgba.
 *
 */
void
gstyle_color_panel_get_rgba (GstyleColorPanel *self,
                             GdkRGBA          *rgba)
{
  g_return_if_fail (GSTYLE_IS_COLOR_PANEL (self));

  gstyle_color_plane_get_rgba (self->color_plane, rgba);
  rgba->alpha = gtk_adjustment_get_value (self->adj_alpha) / 100.0;
}

/**
 * gstyle_color_panel_get_xyz:
 * @self: a #GstyleColorPanel.
 * @xyz: (out): a #GstyleXYZ adress.
 *
 * Fill @xyz with the current color plane xyz.
 *
 */
void
gstyle_color_panel_get_xyz (GstyleColorPanel *self,
                            GstyleXYZ        *xyz)
{
  g_return_if_fail (GSTYLE_IS_COLOR_PANEL (self));

  gstyle_color_plane_get_xyz (self->color_plane, xyz);
  xyz->alpha = gtk_adjustment_get_value (self->adj_alpha) / 100.0;
}

/**
 * gstyle_color_panel_set_rgba:
 * @self: a #GstyleColorPanel.
 * @rgba: a #GdkRGBA.
 *
 * Set the color plane and sliders to rgba.
 *
 */
void
gstyle_color_panel_set_rgba (GstyleColorPanel *self,
                             const GdkRGBA    *rgba)
{
  g_return_if_fail (GSTYLE_IS_COLOR_PANEL (self));

  gtk_adjustment_set_value (self->adj_alpha, rgba->alpha * 100.0);
  gstyle_color_plane_set_rgba (self->color_plane, rgba);
}

/**
 * gstyle_color_panel_set_xyz:
 * @self: a #GstyleColorPanel.
 * @xyz: a #GstyleXYZ.
 *
 * Set the color plane and sliders to the xyz value.
 *
 */
void
gstyle_color_panel_set_xyz (GstyleColorPanel *self,
                            const GstyleXYZ  *xyz)
{
  g_return_if_fail (GSTYLE_IS_COLOR_PANEL (self));

  gtk_adjustment_set_value (self->adj_alpha, xyz->alpha * 100.0);
  gstyle_color_plane_set_xyz (self->color_plane, xyz);
}

/**
 * gstyle_color_panel_get_palette_widget:
 * @self: a #GstyleColorPanel.
 *
 * Returns: (nullable) (transfer none): The #GstylePaletteWidget used by the panel.
 *
 */
GstylePaletteWidget *
gstyle_color_panel_get_palette_widget (GstyleColorPanel *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_PANEL (self), NULL);

  return self->palette_widget;
}

static void
update_hsv_hue_color_ramp (GstyleColorPanel *self,
                           GstyleColorScale *scale,
                           GdkRGBA          *rgba)
{
  gdouble hue;
  guint32 *data;
  GdkRGBA dst_rgba = {0};

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      hue = x * HSV_TO_SCALE_FACTOR;
      gstyle_color_convert_hsv_to_rgb (hue, 1.0, 1.0, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_hsv_saturation_color_ramp (GstyleColorPanel *self,
                                  GstyleColorScale *scale,
                                  GdkRGBA          *rgba)
{
  gdouble hue = 0.0;
  gdouble saturation = 0.0;
  gdouble value = 0.0;
  guint32 *data;
  GdkRGBA dst_rgba = {0};

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  gstyle_color_convert_rgb_to_hsv (rgba, &hue, &saturation, &value);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      saturation = x * HSV_TO_SCALE_FACTOR;
      gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_hsv_value_color_ramp (GstyleColorPanel *self,
                             GstyleColorScale *scale,
                             GdkRGBA          *rgba)
{
  gdouble hue = 0.0;
  gdouble saturation = 0.0;
  gdouble value = 0.0;
  guint32 *data;
  GdkRGBA dst_rgba = {0};

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  gstyle_color_convert_rgb_to_hsv (rgba, &hue, &saturation, &value);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      value = x * HSV_TO_SCALE_FACTOR;
      gstyle_color_convert_hsv_to_rgb (hue, saturation, value, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_rgb_red_color_ramp (GstyleColorPanel *self,
                           GstyleColorScale *scale,
                           GdkRGBA          *rgba)
{
  guint32 *data;
  GdkRGBA dst_rgba = *rgba;

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      dst_rgba.red = x / 256.0;
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_rgb_green_color_ramp (GstyleColorPanel *self,
                             GstyleColorScale *scale,
                             GdkRGBA          *rgba)
{
  guint32 *data;
  GdkRGBA dst_rgba = *rgba;

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      dst_rgba.green = x / 256.0;
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_rgb_blue_color_ramp (GstyleColorPanel *self,
                            GstyleColorScale *scale,
                            GdkRGBA          *rgba)
{
  guint32 *data;
  GdkRGBA dst_rgba = *rgba;

  /* TODO: malloc in init and keep data around */
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      dst_rgba.blue = x / 256.0;
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_lab_l_color_ramp (GstyleColorPanel *self,
                         GstyleColorScale *scale,
                         GdkRGBA          *rgba)
{
  GstyleCielab lab;
  GdkRGBA dst_rgba = {0};
  guint32 *data;

  /* TODO: malloc in init and keep data around */
  gstyle_color_convert_rgb_to_cielab (rgba, &lab);
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      lab.l = x * CIELAB_L_TO_SCALE_FACTOR;
      gstyle_color_convert_cielab_to_rgb (&lab, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_lab_a_color_ramp (GstyleColorPanel *self,
                         GstyleColorScale *scale,
                         GdkRGBA          *rgba)
{
  GstyleCielab lab;
  GdkRGBA dst_rgba = {0};
  guint32 *data;

  /* TODO: malloc in init and keep data around */
  gstyle_color_convert_rgb_to_cielab (rgba, &lab);
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      lab.a = x - 128;
      gstyle_color_convert_cielab_to_rgb (&lab, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_lab_b_color_ramp (GstyleColorPanel *self,
                         GstyleColorScale *scale,
                         GdkRGBA          *rgba)
{
  GstyleCielab lab;
  GdkRGBA dst_rgba = {0};
  guint32 *data;

  /* TODO: malloc in init and keep data around */
  gstyle_color_convert_rgb_to_cielab (rgba, &lab);
  data = g_malloc0 (GSTYLE_COLOR_SCALE_CUSTOM_DATA_BYTE_SIZE);
  for (gint x = 0; x < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++x)
    {
      lab.b = x - 128;
      gstyle_color_convert_cielab_to_rgb (&lab, &dst_rgba);
      data [x] = pack_rgba24 (&dst_rgba);
    }

  gstyle_color_scale_set_custom_data (scale, data);
  g_free (data);
}

static void
update_ref_color_ramp (GstyleColorPanel *self,
                       GdkRGBA          *rgba)
{
  switch (self->current_comp)
    {
    case GSTYLE_COLOR_COMPONENT_HSV_H:
      update_hsv_hue_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_HSV_S:
      update_hsv_saturation_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_HSV_V:
      update_hsv_value_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_LAB_L:
      update_lab_l_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_LAB_A:
      update_lab_a_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_LAB_B:
      update_lab_b_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_RGB_RED:
      update_rgb_red_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_RGB_GREEN:
      update_rgb_green_color_ramp (self, self->ref_scale, rgba);
      break;

    case GSTYLE_COLOR_COMPONENT_RGB_BLUE:
      update_rgb_blue_color_ramp (self, self->ref_scale, rgba);
      break;

    case N_GSTYLE_COLOR_COMPONENT:
    case GSTYLE_COLOR_COMPONENT_NONE:
    default:
      break;
    }
}

static void
color_picked_cb (GstyleColorPanel *self,
                 GdkRGBA          *rgba)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  gstyle_color_plane_set_rgba (self->color_plane, rgba);
}

static void
grab_released_cb (GstyleColorPanel *self)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  g_clear_object (&self->eyedropper);
}

static void
picker_button_clicked_cb (GstyleColorPanel *self,
                          GtkButton        *picker_button)
{
  GdkEvent *event;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GTK_IS_BUTTON (picker_button));

  event = gtk_get_current_event ();
  g_assert (event != NULL);

  self->eyedropper = g_object_ref_sink (g_object_new (GSTYLE_TYPE_EYEDROPPER,
                                        "source-event", event,
                                        NULL));
  gdk_event_free (event);

  g_signal_connect_object (self->eyedropper,
                           "color-picked",
                           G_CALLBACK (color_picked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->eyedropper,
                           "grab-released",
                           G_CALLBACK (grab_released_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
palette_row_activated_cb (GstyleColorPanel    *self,
                          GstylePalette       *palette,
                          gint                 position,
                          GstylePaletteWidget *palette_widget)
{
  const GstyleColor *color;
  GdkRGBA rgba = {0};

  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GSTYLE_IS_PALETTE (palette));
  g_assert (GSTYLE_IS_PALETTE_WIDGET (palette_widget));

  color = gstyle_palette_get_color_at_index (palette, position);
  gstyle_color_fill_rgba ((GstyleColor *)color, &rgba);

  gstyle_color_panel_set_rgba (self, &rgba);
}

static void
update_sub_panels (GstyleColorPanel *self,
                   GdkRGBA           rgba)
{
   g_assert (GSTYLE_IS_COLOR_PANEL (self));

  rgba.alpha = gtk_adjustment_get_value (self->adj_alpha) / 100.0;
  gstyle_color_set_rgba (self->old_color, &rgba);
  update_color_strings (self, gstyle_color_widget_get_filtered_color (self->old_swatch));

  rgba.alpha = 1.0;
  /* TODO: compare with old value and update only when needed */

  update_hsv_hue_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_HSV_H].scale, &rgba);
  update_hsv_saturation_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_HSV_S].scale, &rgba);
  update_hsv_value_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_HSV_V].scale, &rgba);

  update_rgb_red_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_RGB_RED].scale, &rgba);
  update_rgb_green_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_RGB_GREEN].scale, &rgba);
  update_rgb_blue_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_RGB_BLUE].scale, &rgba);

  update_lab_l_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_LAB_L].scale, &rgba);
  update_lab_a_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_LAB_A].scale, &rgba);
  update_lab_b_color_ramp (self, self->components [GSTYLE_COLOR_COMPONENT_LAB_B].scale, &rgba);

  update_ref_color_ramp (self, &rgba);
}

static void
component_toggled_cb (GstyleColorPanel *self,
                      GtkToggleButton  *toggle)
{
  GdkRGBA rgba = {0};
  GtkAdjustment *adj;

  if (!gtk_toggle_button_get_active (toggle))
    {
      gtk_toggle_button_set_active (toggle, TRUE);
      return;
    }

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    {
      if (toggle != self->components [i].toggle)
        {
          g_signal_handler_block (self->components [i].toggle, self->components [i].toggle_handler_id);
          gtk_toggle_button_set_active (self->components [i].toggle, FALSE);
          g_signal_handler_unblock (self->components [i].toggle, self->components [i].toggle_handler_id);
        }
      else
        {
          self->current_comp = i;
          gstyle_color_plane_set_mode (self->color_plane, component_to_plane_mode [i]);
          adj = gstyle_color_plane_get_component_adjustment (self->color_plane, i);
          gtk_range_set_adjustment (GTK_RANGE (self->ref_scale), adj);

          gstyle_color_plane_get_rgba (self->color_plane, &rgba);
          update_ref_color_ramp (self, &rgba);
        }
    }
}

static gint
search_strings_list_sort_func (GtkListBoxRow *row1,
                               GtkListBoxRow *row2,
                               gpointer       user_data)
{
  GstyleColorWidget *cw1;
  GstyleColorWidget *cw2;
  const gchar *name1;
  const gchar *name2;

  cw1 = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (row1)));
  name1 = gstyle_color_get_name (gstyle_color_widget_get_color (cw1));

  cw2 = GSTYLE_COLOR_WIDGET (gtk_bin_get_child (GTK_BIN (row2)));
  name2 = gstyle_color_get_name (gstyle_color_widget_get_color (cw2));

  return g_strcmp0 (name1, name2);
}

static void
search_list_add_color (GstyleColorPanel *self,
                       GstyleColor      *color)
{
  GtkWidget *color_widget;

  color_widget = g_object_new (GSTYLE_TYPE_COLOR_WIDGET,
                               "color", color,
                               "visible", TRUE,
                               "halign", GTK_ALIGN_FILL,
                               NULL);

  gtk_list_box_insert (GTK_LIST_BOX (self->search_strings_list), color_widget, -1);
}

static void
search_color_entry_changed_cb (GstyleColorPanel *self,
                               GtkSearchEntry   *entry)
{
  GPtrArray *ar, *ar_palette;
  GstyleColor *color;
  const gchar *str;
  GList *children;
  GList *l;
  gint sum = 0;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  str = gtk_entry_get_text(GTK_ENTRY(entry));
  if (gstyle_str_empty0 (str))
    {
      gtk_widget_set_visible (self->search_strings_popover, FALSE);
      return;
    }

  children = gtk_container_get_children (GTK_CONTAINER (self->search_strings_list));
  for (l = children; l != NULL; l = g_list_next (l))
    gtk_widget_destroy (GTK_WIDGET (l->data));

  if (str[0] == '#' || g_str_has_prefix (str, "rgb") || g_str_has_prefix (str, "hsl"))
    {
      color = gstyle_color_new_from_string (NULL, str);
      if (color != NULL)
        {
          search_list_add_color (self, color);
          gtk_widget_set_visible (self->search_strings_popover, TRUE);
        }
    }
  else
    {
      ar = gstyle_color_fuzzy_parse_color_string (str);
      if (ar != NULL)
        {
          sum += ar->len;
          for (gint i = 0; i < ar->len; ++i)
            {
              color = g_ptr_array_index (ar, i);
              search_list_add_color (self, color);
            }
        }

      ar_palette = gstyle_palette_widget_fuzzy_parse_color_string (self->palette_widget, str);
      if (ar_palette != NULL && ar_palette->len > 0)
        {
          sum += ar_palette->len;
          for (gint i = 0; i < ar_palette->len; ++i)
            {
              color = g_ptr_array_index (ar_palette, i);
              if (ar == NULL || !gstyle_utils_is_array_contains_same_color (ar, color))
                search_list_add_color (self, color);
            }
        }

      g_ptr_array_unref (ar);
      g_ptr_array_unref (ar_palette);
      gtk_widget_set_visible (self->search_strings_popover, (sum > 0));
    }
}

static GIcon *
get_degree_icon (GstyleColorPanel *self)
{
  GIcon *icon;
  g_autoptr (GFile) file = NULL;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  file = g_file_new_for_uri ("resource:///org/gnome/libgstyle/icons/unit-degree-symbolic.svg");
  icon = g_file_icon_new (file);

  return icon;
}

static GIcon *
get_percent_icon (GstyleColorPanel *self)
{
  GIcon *icon;
  g_autoptr (GFile) file = NULL;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  file = g_file_new_for_uri ("resource:///org/gnome/libgstyle/icons/unit-percent-symbolic.svg");
  icon = g_file_icon_new (file);

  return icon;
}

static void
set_preferred_unit (GstyleColorPanel *self,
                   GstyleColorUnit  preferred_unit)
{
  GIcon *icon = NULL;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  if (self->preferred_unit != preferred_unit)
    {
      self->preferred_unit = preferred_unit;
      if (preferred_unit == GSTYLE_COLOR_UNIT_PERCENT)
        icon = self->percent_icon;
      else if (preferred_unit == GSTYLE_COLOR_UNIT_VALUE)
        icon = NULL;
      else
        g_assert_not_reached ();

      gstyle_color_plane_set_preferred_unit (self->color_plane, preferred_unit);

      gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_RGB_RED].spin),
                                     GTK_ENTRY_ICON_SECONDARY, icon);
      gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_RGB_GREEN].spin),
                                     GTK_ENTRY_ICON_SECONDARY, icon);
      gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_RGB_BLUE].spin),
                                     GTK_ENTRY_ICON_SECONDARY, icon);
    }
}

/* TODO: only keep one transformation func ? */
static gboolean
rgba_plane_to_new_color (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      user_data)
{
  GstyleColorPanel *self = (GstyleColorPanel *)user_data;
  GdkRGBA *rgba = {0};

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  rgba = g_value_get_boxed (from_value);
  rgba->alpha = 1.0;
  g_value_set_boxed (to_value, rgba);

  update_sub_panels (self, *rgba);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_XYZ]);

  return TRUE;
}

static gboolean
rgba_new_color_to_plane (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      user_data)
{
  GstyleColorPanel *self = (GstyleColorPanel *)user_data;
  GdkRGBA *rgba = {0};

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  rgba = g_value_get_boxed (from_value);
  rgba->alpha = 1.0;
  g_value_set_boxed (to_value, rgba);

  update_sub_panels (self, *rgba);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RGBA]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_XYZ]);

  return TRUE;
}

static void
prefs_button_notify_active_cb (GstyleColorPanel *self,
                               GParamSpec       *pspec,
                               GtkToggleButton  *button)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GTK_IS_TOGGLE_BUTTON (button));

  if (gtk_toggle_button_get_active (button))
    self->last_checked_prefs_button = button;
  else
    self->last_checked_prefs_button = NULL;
}

static void
slide_is_closing_cb (GstyleColorPanel *self,
                     GstyleSlidein    *slidein)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GSTYLE_IS_SLIDEIN (slidein));

  if (self->last_checked_prefs_button != NULL)
    gtk_toggle_button_set_active (self->last_checked_prefs_button, FALSE);
}

static void
update_palette_name (GstyleColorPanel *self,
                     GstylePalette    *palette)
{
  const gchar *name;
  g_autofree gchar *full_name = NULL;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (palette == NULL || GSTYLE_IS_PALETTE (palette));

  if (palette != NULL &&
      (name = gstyle_palette_get_name (palette)) &&
      !gstyle_str_empty0 (name))
    full_name = g_strconcat (_("Palette: "), name, NULL);
  else
    full_name = g_strdup (_("Palette"));

  gtk_button_set_label (GTK_BUTTON (self->palette_toggle), full_name);
}

static void
palette_selected_notify_cb (GstyleColorPanel    *self,
                            GParamSpec          *pspec,
                            GstylePaletteWidget *palette_widget)
{
  GstylePalette *palette;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GSTYLE_IS_PALETTE_WIDGET (palette_widget));

  palette = gstyle_palette_widget_get_selected_palette (palette_widget);
  update_palette_name (self, palette);
}

void
_gstyle_color_panel_update_prefs_page (GstyleColorPanel *self,
                                       const gchar      *page_name)
{
  GstyleColorPanelPrefs prefs_type = GSTYLE_COLOR_PANEL_PREFS_COMPONENTS;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  if (g_strcmp0 (page_name, PREFS_COMPONENTS_PAGE) == 0)
    prefs_type = GSTYLE_COLOR_PANEL_PREFS_COMPONENTS;
  else if (g_strcmp0 (page_name, PREFS_COLOR_STRINGS_PAGE) == 0)
    prefs_type = GSTYLE_COLOR_PANEL_PREFS_COLOR_STRINGS;
  else if (g_strcmp0 (page_name, PREFS_PALETTES_PAGE) == 0)
    prefs_type = GSTYLE_COLOR_PANEL_PREFS_PALETTES;
  else if (g_strcmp0 (page_name, PREFS_PALETTES_LIST_PAGE) == 0)
    prefs_type = GSTYLE_COLOR_PANEL_PREFS_PALETTES_LIST;
  else
    g_assert_not_reached ();

  g_signal_emit (self, signals [UPDATE_PREFS], 0, prefs_type);
}

static void
replace_prefs_page (GstyleColorPanel  *self,
                    GtkWidget         *new_page,
                    GtkWidget        **bin,
                    const gchar       *page_name)
{
  if (*bin != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (self->prefs_stack), *bin);
      *bin = NULL;
    }

  if (new_page != NULL)
    {
      *bin = new_page;
      gtk_stack_add_named (self->prefs_stack, new_page, page_name);
    }
}

void
gstyle_color_panel_set_prefs_pages (GstyleColorPanel *self,
                                    GtkWidget        *components_page,
                                    GtkWidget        *colorstrings_page,
                                    GtkWidget        *palettes_page,
                                    GtkWidget        *palettes_list_page)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (components_page == NULL || GTK_IS_WIDGET (components_page));
  g_assert (colorstrings_page == NULL || GTK_IS_WIDGET (colorstrings_page));
  g_assert (palettes_page == NULL || GTK_IS_WIDGET (palettes_page));
  g_assert (palettes_list_page == NULL || GTK_IS_WIDGET (palettes_list_page));

  replace_prefs_page (self, components_page, &self->components_prefs_bin, PREFS_COMPONENTS_PAGE);
  replace_prefs_page (self, colorstrings_page, &self->color_strings_prefs_bin, PREFS_COLOR_STRINGS_PAGE);
  replace_prefs_page (self, palettes_page, &self->palettes_prefs_bin, PREFS_PALETTES_PAGE);
  replace_prefs_page (self, palettes_list_page, &self->palettes_list_prefs_bin, PREFS_PALETTES_LIST_PAGE);
}

static void
bar_toggled_cb (GtkToggleButton *toggle,
                GstyleRevealer  *reveal)
{
  GtkStyleContext *context;
  gboolean active;

  g_assert (GTK_IS_TOGGLE_BUTTON (toggle));
  g_assert (GSTYLE_IS_REVEALER (reveal));

  context = gtk_widget_get_style_context (GTK_WIDGET (toggle));
  active  = gtk_toggle_button_get_active (toggle);
  gstyle_revealer_set_reveal_child (reveal, active);

  if (active)
    gtk_style_context_remove_class (context, "dim-label");
  else
    gtk_style_context_add_class (context, "dim-label");
}

static void
setup_ui (GstyleColorPanel *self)
{
  GdkRGBA rgba = {0.26, 0.5, 0.5};
  GtkAdjustment *adj;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    {
      g_autofree gchar *toggle = g_strconcat (comp_names [i], "_toggle", NULL);
      g_autofree gchar *spin = g_strconcat (comp_names [i], "_spin", NULL);
      g_autofree gchar *scale = g_strconcat (comp_names [i], "_scale", NULL);

      self->components [i].toggle =
        GTK_TOGGLE_BUTTON (gtk_widget_get_template_child (GTK_WIDGET (self),
                                                          GSTYLE_TYPE_COLOR_PANEL,
                                                          toggle));
      self->components [i].spin =
        GTK_SPIN_BUTTON (gtk_widget_get_template_child (GTK_WIDGET (self),
                                                        GSTYLE_TYPE_COLOR_PANEL,
                                                        spin));
      self->components [i].scale =
        GSTYLE_COLOR_SCALE (gtk_widget_get_template_child (GTK_WIDGET (self),
                                                           GSTYLE_TYPE_COLOR_PANEL,
                                                           scale));

      adj = gstyle_color_plane_get_component_adjustment (self->color_plane, i);
      gtk_range_set_adjustment (GTK_RANGE (self->components [i].scale), adj);
      gtk_spin_button_set_adjustment (self->components [i].spin, adj);

      self->components [i].toggle_handler_id =
        g_signal_connect_swapped (self->components [i].toggle,
                                  "toggled",
                                  G_CALLBACK (component_toggled_cb),
                                  self);
    }

  self->current_comp = GSTYLE_COLOR_COMPONENT_HSV_H;
  gtk_toggle_button_set_active (self->components [GSTYLE_COLOR_COMPONENT_HSV_H].toggle, TRUE);

  self->adj_alpha = gtk_adjustment_new (50.0, 0.0, 100.0, 0.1, 1.0, 0.0);
  gtk_range_set_adjustment (GTK_RANGE (self->alpha_scale), self->adj_alpha);
  g_signal_connect_swapped (self->adj_alpha, "value-changed",
                            G_CALLBACK (adj_alpha_value_changed_cb), self);

  self->new_color = gstyle_color_new (NULL, GSTYLE_COLOR_KIND_RGB_HEX6, 0.0, 0.0, 0.0, 100);
  gstyle_color_widget_set_color (self->new_swatch, self->new_color);
  g_object_set (self->new_swatch,
                "dnd-lock", GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALPHA |
                            GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_KIND |
                            GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_NAME,
                NULL);

  self->old_color = gstyle_color_new (NULL, GSTYLE_COLOR_KIND_RGB_HEX6, 0.0, 0.0, 0.0, 50);
  gstyle_color_widget_set_color (self->old_swatch, self->old_color);
  g_object_set (self->old_swatch,
                "dnd-lock", GSTYLE_COLOR_WIDGET_DND_LOCK_FLAGS_ALL,
                NULL);

  bar_toggled_cb (self->components_toggle, self->scale_reveal);
  g_signal_connect (self->components_toggle, "toggled",
                    G_CALLBACK (bar_toggled_cb), self->scale_reveal);

  bar_toggled_cb (self->strings_toggle, self->string_reveal);
  g_signal_connect (self->strings_toggle, "toggled",
                    G_CALLBACK (bar_toggled_cb), self->string_reveal);

  bar_toggled_cb (self->palette_toggle, self->palette_reveal);
  g_signal_connect (self->palette_toggle, "toggled",
                    G_CALLBACK (bar_toggled_cb), self->palette_reveal);

  /* default value to start with */
  g_object_bind_property_full (self->color_plane, "rgba", self->new_color,
                               "rgba", G_BINDING_BIDIRECTIONAL,
                               rgba_plane_to_new_color, rgba_new_color_to_plane,
                               self, NULL);
  gstyle_color_plane_set_rgba (self->color_plane, &rgba);

  gtk_popover_set_relative_to (GTK_POPOVER (self->search_strings_popover),
                               GTK_WIDGET (self->search_color_entry));
  g_signal_connect_swapped (self->search_color_entry, "search-changed",
                            G_CALLBACK (search_color_entry_changed_cb),
                            self);

  gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_HSV_H].spin),
                                 GTK_ENTRY_ICON_SECONDARY, self->degree_icon);
  gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_HSV_S].spin),
                                 GTK_ENTRY_ICON_SECONDARY, self->percent_icon);
  gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_HSV_V].spin),
                                 GTK_ENTRY_ICON_SECONDARY, self->percent_icon);
  gtk_entry_set_icon_from_gicon (GTK_ENTRY (self->components [GSTYLE_COLOR_COMPONENT_LAB_L].spin),
                                 GTK_ENTRY_ICON_SECONDARY, self->percent_icon);
  set_preferred_unit (self, GSTYLE_COLOR_UNIT_VALUE);

  g_signal_connect_swapped (self->palette_widget, "activated",
                            G_CALLBACK (palette_row_activated_cb),
                            self);
  g_signal_connect_swapped (self->palette_widget, "notify::selected-palette-id",
                            G_CALLBACK (palette_selected_notify_cb),
                            self);

  g_signal_connect_swapped (self->picker_button, "clicked",
                            G_CALLBACK (picker_button_clicked_cb),
                            self);

  g_signal_connect_swapped (self->prefs_slidein, "closing",
                            G_CALLBACK (slide_is_closing_cb),
                            self);

  g_signal_connect_swapped (self->components_prefs_button,
                            "notify::active",
                            G_CALLBACK (prefs_button_notify_active_cb),
                            self);
  g_signal_connect_swapped (self->color_strings_prefs_button,
                            "notify::active",
                            G_CALLBACK (prefs_button_notify_active_cb),
                            self);
  g_signal_connect_swapped (self->palettes_prefs_button,
                            "notify::active",
                            G_CALLBACK (prefs_button_notify_active_cb),
                            self);
  g_signal_connect_swapped (self->palettes_list_prefs_button,
                            "notify::active",
                            G_CALLBACK (prefs_button_notify_active_cb),
                            self);
}

/**
 * gstyle_color_panel_show_palette:
 * @self: a #GstyleColorPanel.
 * @palette: A GstylePalette.
 *
 * Show the @palette and update its name in the bar.
 *
 */
void
gstyle_color_panel_show_palette (GstyleColorPanel *self,
                                 GstylePalette    *palette)
{
  g_assert (GSTYLE_IS_COLOR_PANEL (self));
  g_assert (GSTYLE_IS_PALETTE (palette));

  if(gstyle_palette_widget_show_palette (self->palette_widget, palette))
    update_palette_name (self, palette);
}

static void
gstyle_color_panel_set_strings_visible (GstyleColorPanel                    *self,
                                        GstyleColorPanelStringsVisibleFlags  flags)
{
  gboolean hex3_visible;
  gboolean hex6_visible;
  gboolean rgb_visible;
  gboolean rgba_visible;
  gboolean hsl_visible;
  gboolean hsla_visible;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  if (self->strings_visible_flags != flags)
    {
      self->strings_visible_flags = flags;

      hex3_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX3);
      hex6_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX6);
      rgb_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGB);
      rgba_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGBA);
      hsl_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSL);
      hsla_visible = !!(flags & GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSLA);

      gtk_widget_set_visible (GTK_WIDGET (self->hex3_label), hex3_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_hex3_label), hex3_visible);

      gtk_widget_set_visible (GTK_WIDGET (self->hex6_label), hex6_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_hex6_label), hex6_visible);

      gtk_widget_set_visible (GTK_WIDGET (self->rgb_label), rgb_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_rgb_label), rgb_visible);

      gtk_widget_set_visible (GTK_WIDGET (self->rgba_label), rgba_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_rgba_label), rgba_visible);

      gtk_widget_set_visible (GTK_WIDGET (self->hsl_label), hsl_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_hsl_label), hsl_visible);

      gtk_widget_set_visible (GTK_WIDGET (self->hsla_label), hsla_visible);
      gtk_widget_set_visible (GTK_WIDGET (self->res_hsla_label), hsla_visible);
    }
}

GstyleColorPanel *
gstyle_color_panel_new (void)
{
  return g_object_new (GSTYLE_TYPE_COLOR_PANEL, NULL);
}

static void
gstyle_color_panel_dispose (GObject *object)
{
  GstyleColorPanel *self = (GstyleColorPanel *)object;

  g_clear_object (&self->new_color);
  g_clear_object (&self->default_provider);
  g_clear_object (&self->degree_icon);
  g_clear_object (&self->percent_icon);
  g_clear_object (&self->eyedropper);
  gstyle_color_panel_set_prefs_pages (self, NULL, NULL, NULL, NULL);

  G_OBJECT_CLASS (gstyle_color_panel_parent_class)->dispose (object);
}

static void
gstyle_color_panel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GstyleColorPanel *self = GSTYLE_COLOR_PANEL (object);
  GdkRGBA rgba = {0};
  GstyleXYZ xyz;

  switch (prop_id)
    {
    case PROP_FILTER:
      g_value_set_enum (value, gstyle_color_panel_get_filter (self));
      break;

    case PROP_HSV_VISIBLE:
      g_value_set_boolean (value, gtk_widget_get_visible (self->hsv_grid));
      break;

    case PROP_LAB_VISIBLE:
      g_value_set_boolean (value, gtk_widget_get_visible (self->lab_grid));
      break;

    case PROP_RGB_VISIBLE:
      g_value_set_boolean (value, gtk_widget_get_visible (self->rgb_grid));
      break;

    case PROP_STRINGS_VISIBLE:
      g_value_set_flags (value, self->strings_visible_flags);
      break;

    case PROP_RGBA:
      gstyle_color_panel_get_rgba (self, &rgba);
      g_value_set_boxed (value, &rgba);
      break;

    case PROP_RGB_UNIT:
      g_value_set_enum (value, self->preferred_unit);
      break;

    case PROP_XYZ:
      gstyle_color_panel_get_xyz (self, &xyz);
      g_value_set_boxed (value, &xyz);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_panel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GstyleColorPanel *self = GSTYLE_COLOR_PANEL (object);
  GdkRGBA *rgba_p;
  GstyleXYZ *xyz_p;

  switch (prop_id)
    {
    case PROP_FILTER:
      gstyle_color_panel_set_filter (self, g_value_get_enum (value));
      break;

    case PROP_HSV_VISIBLE:
      gtk_widget_set_visible (self->hsv_grid, g_value_get_boolean (value));
      break;

    case PROP_LAB_VISIBLE:
      gtk_widget_set_visible (self->lab_grid, g_value_get_boolean (value));
      break;

    case PROP_RGB_VISIBLE:
      gtk_widget_set_visible (self->rgb_grid, g_value_get_boolean (value));
      break;

    case PROP_STRINGS_VISIBLE:
      gstyle_color_panel_set_strings_visible (self, g_value_get_flags (value));
      break;

    case PROP_RGB_UNIT:
      set_preferred_unit (self, g_value_get_enum (value));
      break;

    case PROP_RGBA:
      rgba_p = (GdkRGBA *)g_value_get_boxed (value);
      if (rgba_p != NULL)
        gstyle_color_panel_set_rgba (self, rgba_p);
      break;

    case PROP_XYZ:
      xyz_p = (GstyleXYZ *)g_value_get_boxed (value);
      if (xyz_p != NULL)
        gstyle_color_panel_set_xyz (self, xyz_p);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_panel_class_init (GstyleColorPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gstyle_color_panel_dispose;
  object_class->get_property = gstyle_color_panel_get_property;
  object_class->set_property = gstyle_color_panel_set_property;

  g_resources_register (gstyle_get_resource ());

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/libgstyle/ui/gstyle-color-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, new_swatch);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, old_swatch);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, color_plane);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, hsv_grid);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, lab_grid);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, rgb_grid);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, components_controls);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, strings_controls);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palette_controls);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, components_toggle);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, strings_toggle);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palette_toggle);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, scale_reveal);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, string_reveal);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palette_reveal);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, ref_scale);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, alpha_scale);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palette_widget);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_hex3_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_hex6_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_rgb_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_rgba_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_hsl_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, res_hsla_label);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, hex3_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, hex6_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, rgb_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, rgba_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, hsl_label);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, hsla_label);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, prefs_stack);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, components_prefs_button);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, color_strings_prefs_button);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palettes_prefs_button);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, palettes_list_prefs_button);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, search_color_entry);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, search_strings_popover);
  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, search_strings_list);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, picker_button);

  gtk_widget_class_bind_template_child (widget_class, GstyleColorPanel, prefs_slidein);

  g_type_ensure (GSTYLE_TYPE_SLIDEIN);
  g_type_ensure (GSTYLE_TYPE_COLOR_SCALE);
  g_type_ensure (GSTYLE_TYPE_COLOR_PLANE);
  g_type_ensure (GSTYLE_TYPE_COLOR_WIDGET);
  g_type_ensure (GSTYLE_TYPE_REVEALER);
  g_type_ensure (GSTYLE_TYPE_PALETTE_WIDGET);

  for (gint i = 0; i < N_GSTYLE_COLOR_COMPONENT; ++i)
    {
      g_autofree gchar *toggle = g_strconcat (comp_names [i], "_toggle", NULL);
      g_autofree gchar *spin = g_strconcat (comp_names [i], "_spin", NULL);
      g_autofree gchar *scale = g_strconcat (comp_names [i], "_scale", NULL);

      gtk_widget_class_bind_template_child_full (widget_class, toggle, FALSE, 0);
      gtk_widget_class_bind_template_child_full (widget_class, spin, FALSE, 0);
      gtk_widget_class_bind_template_child_full (widget_class, scale, FALSE, 0);
    }

  gtk_widget_class_set_css_name (widget_class, "gstylecolorpanel");

  properties [PROP_FILTER] =
    g_param_spec_enum ("filter",
                       "filter",
                       "Filer used to act on color scales and plane",
                       GSTYLE_TYPE_COLOR_FILTER,
                       GSTYLE_COLOR_FILTER_NONE,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RGBA] =
    g_param_spec_boxed ("rgba",
                        "rgba",
                        "current color of the color plane",
                        GDK_TYPE_RGBA,
                        (G_PARAM_CONSTRUCT |
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_XYZ] =
    g_param_spec_boxed ("xyz",
                        "xyz",
                        "current xyz color of the color plane",
                        GSTYLE_TYPE_XYZ,
                        (G_PARAM_CONSTRUCT |
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_HSV_VISIBLE] =
    g_param_spec_boolean ("hsv-visible",
                          "hsv-visible",
                          "Visibility of the HSV components",
                          TRUE,
                          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAB_VISIBLE] =
    g_param_spec_boolean ("lab-visible",
                          "lab-visible",
                          "Visibility of the LAB components",
                          TRUE,
                          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RGB_VISIBLE] =
    g_param_spec_boolean ("rgb-visible",
                          "rgb-visible",
                          "Visibility of the RGB components",
                          TRUE,
                          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RGB_UNIT] =
    g_param_spec_enum ("rgb-unit",
                       "rgb-unit",
                       "Units used by the RGB components and strings",
                       GSTYLE_TYPE_COLOR_UNIT,
                       GSTYLE_COLOR_UNIT_PERCENT,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_STRINGS_VISIBLE] =
    g_param_spec_flags ("strings-visible",
                        "strings-visible",
                        "Color strings visible",
                        GSTYLE_TYPE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS,
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX3 |
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX6 |
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGB  |
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGBA |
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSL  |
                        GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSLA ,
                        (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [UPDATE_PREFS] = g_signal_new ("update-prefs",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          1,
                                          GSTYLE_TYPE_COLOR_PANEL_PREFS);

  /* signals [DISCONNECT_PREFS] = g_signal_new ("disconnect-prefs", */
  /*                                            G_TYPE_FROM_CLASS (klass), */
  /*                                            G_SIGNAL_RUN_LAST, */
  /*                                            0, */
  /*                                            NULL, NULL, NULL, */
  /*                                            G_TYPE_NONE, */
  /*                                            2, */
  /*                                            GSTYLE_TYPE_COLOR_PANEL_PREFS, */
  /*                                            GTK_TYPE_WIDGET); */
}

static void
gstyle_color_panel_init (GstyleColorPanel *self)
{
  GtkStyleContext *context;

  gtk_widget_init_template (GTK_WIDGET (self));
  gstyle_color_panel_actions_init (self);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                                    "/org/gnome/libgstyle/icons");

  self->degree_icon = get_degree_icon (self);
  self->percent_icon = get_percent_icon (self);

  self->preferred_unit = GSTYLE_COLOR_UNIT_VALUE;

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->search_strings_list),
                              search_strings_list_sort_func,
                              self,
                              NULL);
  setup_ui (self);
}

GType
gstyle_color_panel_prefs_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_PANEL_PREFS_COMPONENTS,    "GSTYLE_COLOR_PANEL_PREFS_COMPONENTS",    "components" },
    { GSTYLE_COLOR_PANEL_PREFS_COLOR_STRINGS, "GSTYLE_COLOR_PANEL_PREFS_COLOR_STRINGS", "colorstrings" },
    { GSTYLE_COLOR_PANEL_PREFS_PALETTES,      "GSTYLE_COLOR_PANEL_PREFS_PALETTES",      "palettes" },
    { GSTYLE_COLOR_PANEL_PREFS_PALETTES_LIST, "GSTYLE_COLOR_PANEL_PREFS_PALETTES_LIST", "paletteslist" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorPanelPrefs", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

GType
gstyle_color_panel_strings_visible_flags_get_type (void)
{
  static GType type_id;
  static const GFlagsValue values[] = {
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_NONE, "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_NONE", "none" },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX3, "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX3", "hex3" },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX6, "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HEX6", "hex6" },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGB,  "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGB",  "rgb"  },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGBA, "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_RGBA", "rgba" },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSL,  "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSL",  "hsl"  },
    { GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSLA, "GSTYLE_COLOR_PANEL_STRINGS_VISIBLE_FLAGS_HSLA", "hsla" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_flags_register_static ("GstyleColorPanelStringsVisibleFlags", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
