/* gstyle-color-scale.c
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

#define G_LOG_DOMAIN "gstyle-color-scale"

#include <math.h>
#include <cairo/cairo.h>
#include <glib/gi18n.h>

#include "gstyle-css-provider.h"
#include "gstyle-utils.h"

#include "gstyle-color-scale.h"

static glong id_count = 1;

typedef struct _ColorStop
{
  gint    id;
  gdouble offset;
  GdkRGBA rgba;
} ColorStop;

struct _GstyleColorScale
{
  GtkScale               parent_instance;

  GstyleCssProvider     *default_provider;

  GstyleColorFilterFunc  filter;
  gpointer               filter_user_data;

  GtkGesture            *long_press_gesture;
  GstyleColorScaleKind   kind;
  GSequence             *custom_color_stops;
  cairo_pattern_t       *pattern;
  cairo_pattern_t       *checkered_pattern;

  cairo_surface_t       *data_surface;
  guint32               *data_raw;
  guint32               *data_raw_filtered;
  gint                   data_stride;
};

G_DEFINE_TYPE (GstyleColorScale, gstyle_color_scale, GTK_TYPE_SCALE)

enum {
  PROP_0,
  PROP_KIND,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
hold_action (GtkGestureLongPress *gesture,
             gdouble              x,
             gdouble              y,
             GstyleColorScale    *self)
{
  gboolean handled;

  g_assert (GSTYLE_IS_COLOR_SCALE (self));

  g_signal_emit_by_name (self, "popup-menu", &handled);
}

static void
filter_data (GstyleColorScale *self)
{
  guint32 *src_data = self->data_raw;
  guint32 *dst_data = self->data_raw_filtered;
  GdkRGBA src_rgba;
  GdkRGBA dst_rgba;

  g_assert (GSTYLE_IS_COLOR_SCALE (self));
  g_assert (self->filter != NULL);

  for (gint i = 0; i < GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE; ++i)
    {
      unpack_rgba24 (src_data [i], &src_rgba);
      self->filter (&src_rgba, &dst_rgba, self->filter_user_data);
      dst_data [i] = pack_rgba24 (&dst_rgba);
    }
}

/**
 * gstyle_color_scale_get_filter_func: (skip):
 * @self: a #GstyleColorScale
 *
 * Get a pointer to the current filter function or %NULL
 * if no filter is actually set.
 *
 * Returns: (nullable): A GstyleColorFilterFunc function pointer.
 *
 */
GstyleColorFilterFunc
gstyle_color_scale_get_filter_func (GstyleColorScale *self)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_SCALE (self), NULL);

  return self->filter;
}

/* TODO: do a copy of orignal data so that we can remove or change the filter
 * keeping the original datas */
/**
 * gstyle_color_scale_set_filter_func:
 * @self: a #GstyleColorScale
 * @filter_cb: (scope notified) (nullable): A GstyleColorFilterFunc filter function or
 *   %NULL to unset the current filter. In this case, user_data is ignored.
 * @user_data: (closure) (nullable): user data to pass when calling the filter function
 *
 * Set a filter to be used to change the drawing of the color scale
 * when kind is set to custom-data (GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
 * The data are filtered just after calling gstyle_color_scale_set_custom_data.
 * So that if you remove or change the filter, you need to call it again.
 *
 */
void
gstyle_color_scale_set_filter_func (GstyleColorScale      *self,
                                    GstyleColorFilterFunc  filter_cb,
                                    gpointer               user_data)
{
  g_return_if_fail (GSTYLE_IS_COLOR_SCALE (self));

  if (self->filter != filter_cb)
    {
      self->filter = filter_cb;
      self->filter_user_data = (filter_cb == NULL) ? NULL : user_data;

      if (self->kind != GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
        return;

      cairo_surface_flush (self->data_surface);
      if (filter_cb == NULL)
        memcpy (self->data_raw_filtered, self->data_raw, self->data_stride);
      else
        filter_data (self);

      cairo_surface_mark_dirty (self->data_surface);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

/**
 * gstyle_color_scale_clear_color_stops:
 * @self: a #GstyleColorScale
 *
 * CLear all the color stops from the color scale.
 *
 */
void
gstyle_color_scale_clear_color_stops (GstyleColorScale *self)
{
  g_return_if_fail (GSTYLE_IS_COLOR_SCALE (self));

  g_sequence_free (self->custom_color_stops);
  self->custom_color_stops = g_sequence_new (NULL);
}

static gint
compare_color_stop_by_offset (ColorStop        *a,
                              ColorStop        *b,
                              GstyleColorScale *self)
{
  gdouble delta;

  g_assert (GSTYLE_IS_COLOR_SCALE (self));
  g_assert (b != NULL && a != NULL);

  delta = a->offset - b->offset;
  if (delta < 0)
    return -1;
  else if (delta > 0)
    return 1;
  else
    return 0;
}

/**
 * gstyle_color_scale_remove_color_stop:
 * @self: a #GstyleColorScale
 * @id: id of a color stop as returned by gstyle_color_scale_add_* functions
 *
 * Remove an existing color stop for the color scale.
 *
 * Returns:  %TRUE if the color stop exist and is removed, %FALSE otherwise.
 *
 */
gboolean
gstyle_color_scale_remove_color_stop (GstyleColorScale *self,
                                      gint              id)
{
  g_return_val_if_fail (GSTYLE_IS_COLOR_SCALE (self), FALSE);
  g_return_val_if_fail (id <= 0, FALSE);

  /* TODO: function code */
  return TRUE;
}

/* TODO: use color stops with the data_raw instead of gradient pattern
 * so that we can use filter on that too */

/**
 * gstyle_color_scale_add_rgba_color_stop:
 * @self: a #GstyleColorScale
 * @offset: position in the range [0, 1] of the color stop
 * @rgba: a #GdkRGBA
 *
 * Set a color stop for the color scale.
 * If there's no color stop at offset 0, a black opaque color stop is automatically added.
 * If there's no color stop at offset 1, a white opaque color stop is automatically added.
 *
 * Returns: the id of the color stop or -1 if there's an existing
 * stop color at the same offset
 *
 */
gint
gstyle_color_scale_add_rgba_color_stop (GstyleColorScale *self,
                                        gdouble           offset,
                                        GdkRGBA          *rgba)
{
  ColorStop *color_stop;
  GSequenceIter *iter = NULL;

  g_return_val_if_fail (GSTYLE_IS_COLOR_SCALE (self), -1);
  g_return_val_if_fail (0.0 <= offset && offset <= 1.0, -1);
  g_return_val_if_fail (rgba != NULL, -1);

  color_stop = g_slice_new0 (ColorStop);
  color_stop->id = id_count;
  color_stop->offset = offset;
  color_stop->rgba = *rgba;

  if (!g_sequence_is_empty (self->custom_color_stops))
    iter = g_sequence_lookup (self->custom_color_stops, color_stop,
                              (GCompareDataFunc)compare_color_stop_by_offset,
                              self);

  if (iter != NULL)
    {
      g_slice_free (ColorStop, color_stop);
      return -1;
    }
  else
    {
      g_sequence_insert_sorted (self->custom_color_stops, color_stop,
                                (GCompareDataFunc)compare_color_stop_by_offset,
                                self);
      id_count += 1;

      gstyle_clear_pointer (&self->pattern, cairo_pattern_destroy);

      if (gtk_widget_get_realized (GTK_WIDGET (self)))
        gtk_widget_queue_draw (GTK_WIDGET (self));

      return color_stop->id;
    }
}

/**
 * gstyle_color_scale_set_custom_data:
 * @self: a #GstyleColorScale
 * @data: data location
 *
 * Set the data used to draw the color ramp if your have
 * GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA kind selected.
 *
 * the location point to a memory array of 256 contiguous pixels
 * using the CAIRO_FORMAT_RGB24 format (so 256 * 4 bytes)
 * This result in using an guint32 for each pixel.
 *
 */
void
gstyle_color_scale_set_custom_data (GstyleColorScale *self,
                                    guint32          *data)
{
  g_return_if_fail (GSTYLE_IS_COLOR_SCALE (self));
  g_return_if_fail (data != NULL);

  if (self->kind != GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
    {
      g_warning ("You need to set the kind to custom-data (GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)"
                 " to use this function.");
      return;
    }

  g_assert (self->data_surface != NULL);

  cairo_surface_flush (self->data_surface);
  memcpy (self->data_raw, data, self->data_stride);

  if (self->filter == NULL)
    memcpy (self->data_raw_filtered, self->data_raw, self->data_stride);
  else
    filter_data (self);

  cairo_surface_mark_dirty (self->data_surface);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * gstyle_color_scale_add_color_stop:
 * @self: a #GstyleColorScale
 * @offset: position in the range [0, 1] of the color stop
 * @red: red component of the color stop
 * @green: red component of the color stop
 * @blue: red component of the color stop
 * @alpha: red component of the color stop
 *
 * Set a color stop for the #GstyleColorScale.
 * If there's no color stop at offset 0, a black opaque color stop is automatically added.
 * If there's no color stop at offset 1, a white opaque color stop is automatically added.
 *
 * Returns: the id of the color stop or -1 if there's an existing
 * stop color at the same offset
 *
 */
gint
gstyle_color_scale_add_color_stop (GstyleColorScale *self,
                                   gdouble           offset,
                                   gdouble           red,
                                   gdouble           green,
                                   gdouble           blue,
                                   gdouble           alpha)
{
  GdkRGBA rgba = {red, green, blue, alpha};

  g_return_val_if_fail (GSTYLE_IS_COLOR_SCALE (self), -1);

  return gstyle_color_scale_add_rgba_color_stop (self, offset, &rgba);
}

/**
 * gstyle_color_scale_get_kind:
 * @self: a #GstyleColorScale
 *
 * Get the kind of gradient displayed in the scale.
 *
 * Returns: a #GstyleColorKind.
 *
 */
GstyleColorScaleKind
gstyle_color_scale_get_kind (GstyleColorScale *self)
{
  g_assert (GSTYLE_IS_COLOR_SCALE (self));

  return self->kind;
}

/**
 * gstyle_color_scale_set_kind:
 * @self: a #GstyleColorScale
 * @kind: a #GstyleColorKind
 *
 * Set the kind of gradient displayed in the scale.
 * If you set the kind to GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS,
 * all the previously added color stops are cleared.
 *
 * if you set the kind to GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA,
 * the previous data are freed.
 *
 */
void
gstyle_color_scale_set_kind (GstyleColorScale     *self,
                             GstyleColorScaleKind  kind)
{
  g_return_if_fail (GSTYLE_IS_COLOR_SCALE (self));

  if (self->kind != kind)
    {
      self->kind = kind;
      if (kind == GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS)
        gstyle_color_scale_clear_color_stops (self);
      else if (kind == GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
        {
          self->data_stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24,
                                                             GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE);
          self->data_raw = g_malloc0 (self->data_stride);
          self->data_raw_filtered = g_malloc0 (self->data_stride);
          self->data_surface = cairo_image_surface_create_for_data ((guchar *)self->data_raw_filtered,
                                                                    CAIRO_FORMAT_RGB24,
                                                                    GSTYLE_COLOR_SCALE_CUSTOM_DATA_PIXEL_SIZE, 1,
                                                                    self->data_stride);
        }

      gstyle_clear_pointer (&self->pattern, cairo_pattern_destroy);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);

      if (gtk_widget_get_realized (GTK_WIDGET (self)))
        gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
update_pattern (GstyleColorScale *self)
{
  cairo_pattern_t *pattern;

  g_assert (GSTYLE_IS_COLOR_SCALE (self));

  if (self->kind == GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
    return;

  pattern = cairo_pattern_create_linear (0, 0, 1, 0);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_NONE);

  if (self->kind != GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS)
    {
      switch (self->kind)
        {
        case GSTYLE_COLOR_SCALE_KIND_HUE:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0000, 1.0, 0.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 0.1666, 1.0, 1.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 0.3333, 0.0, 1.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 0.5000, 0.0, 1.0, 1.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 0.6666, 0.0, 0.0, 1.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 0.8333, 1.0, 0.0, 1.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0000, 1.0, 0.0, 0.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_GREY:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0, 1.0, 1.0, 1.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_ALPHA:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 0.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0.0, 0.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_RED:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0, 1.0, 0.0, 0.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_GREEN:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 1.0, 0.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_BLUE:
          cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 1.0);
          cairo_pattern_add_color_stop_rgba (pattern, 1.0, 0.0, 0.0, 1.0, 1.0);
          break;

        case GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS:
        case GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA:
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      GSequenceIter *iter;
      GSequenceIter *iter_offset_zero = NULL;
      GSequenceIter *iter_offset_one = NULL;
      ColorStop local_color_stop;
      gint len;

      if (!g_sequence_is_empty (self->custom_color_stops))
        {
          local_color_stop.offset = 0.0;
          iter_offset_zero = g_sequence_lookup (self->custom_color_stops, &local_color_stop,
                                                (GCompareDataFunc)compare_color_stop_by_offset,
                                                self);

          local_color_stop.offset = 1.0;
          iter_offset_one = g_sequence_lookup (self->custom_color_stops, &local_color_stop,
                                               (GCompareDataFunc)compare_color_stop_by_offset,
                                               self);
        }

      if (iter_offset_zero == NULL)
        cairo_pattern_add_color_stop_rgba (pattern, 0, 0.0, 0.0, 0.0, 1.0);

      if (iter_offset_one == NULL)
        cairo_pattern_add_color_stop_rgba (pattern, 0, 0.0, 0.0, 0.0, 1.0);

      len = g_sequence_get_length (self->custom_color_stops);
      for (gint i = 0; i < len; ++i)
        {
          ColorStop *color_stop;
          GdkRGBA rgba;

          iter = g_sequence_get_iter_at_pos (self->custom_color_stops, i);
          color_stop = g_sequence_get (iter);
          rgba = color_stop->rgba;
          cairo_pattern_add_color_stop_rgba (pattern, color_stop->offset,
                                             rgba.red, rgba.green, rgba.blue, rgba.alpha);
        }
    }

  gstyle_clear_pointer (&self->pattern, cairo_pattern_destroy);
  self->pattern = pattern;
}

static gboolean
gstyle_color_scale_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
  GstyleColorScale *self = (GstyleColorScale *)widget;
  GtkAllocation alloc;
  gboolean inverted;
  GdkRectangle rect;
  cairo_matrix_t matrix;
  cairo_pattern_t *data_pattern;

  g_assert (GSTYLE_IS_COLOR_SCALE (self));
  g_assert (cr != NULL);

  gtk_widget_get_allocation (widget, &alloc);
  gtk_range_get_range_rect (GTK_RANGE (self), &rect);

  cairo_save (cr);
  cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
  cairo_clip (cr);

  cairo_set_source_rgb (cr, 0.20, 0.20, 0.20);
  cairo_paint (cr);
  cairo_set_source_rgb (cr, 0.80, 0.80, 0.80);

  cairo_matrix_init_scale (&matrix, 0.1, 0.1);
  cairo_matrix_translate (&matrix, -rect.x - 1, -rect.y - 1);
  cairo_pattern_set_matrix (self->checkered_pattern, &matrix);
  cairo_mask (cr, self->checkered_pattern);

  cairo_translate (cr, rect.x, rect.y);
  cairo_scale (cr, rect.width, rect.height);

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_VERTICAL)
    {
      cairo_rotate (cr, -G_PI_2);
      cairo_scale (cr, -1, 1);
    }

 inverted = gtk_range_get_inverted (GTK_RANGE (self));
  if (inverted)
    {
      cairo_translate (cr, 1, 0);
      cairo_scale (cr, -1, 1);
    }

  if (self->kind != GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA)
    {
      if (self->pattern == NULL)
        update_pattern (self);

      cairo_set_source (cr, self->pattern);
      cairo_paint (cr);
    }
  else
    {
      cairo_set_source_surface (cr, self->data_surface, 0.0, 0.0);
      data_pattern = cairo_get_source (cr);
      cairo_pattern_set_extend (data_pattern, CAIRO_EXTEND_NONE);
      cairo_pattern_set_filter (data_pattern, CAIRO_FILTER_NEAREST);
      cairo_matrix_init_scale (&matrix, 256.0, 1.0);
      cairo_pattern_set_matrix (data_pattern, &matrix);
      cairo_paint (cr);
    }

  cairo_restore (cr);

  GTK_WIDGET_CLASS (gstyle_color_scale_parent_class)->draw (widget, cr);
  return GDK_EVENT_PROPAGATE;
}

GstyleColorScale *
gstyle_color_scale_new (GtkAdjustment *adjustment)
{
  return g_object_new (GSTYLE_TYPE_COLOR_SCALE,
                       "adjustment", adjustment,
                       NULL);
}

static void
gstyle_color_scale_finalize (GObject *object)
{
  GstyleColorScale *self = (GstyleColorScale *)object;

  g_clear_object (&self->long_press_gesture);
  g_clear_object (&self->default_provider);

  gstyle_clear_pointer (&self->custom_color_stops, g_sequence_free);
  gstyle_clear_pointer (&self->checkered_pattern, cairo_pattern_destroy);
  gstyle_clear_pointer (&self->pattern, cairo_pattern_destroy);
  gstyle_clear_pointer (&self->data_surface, cairo_surface_destroy);

  gstyle_clear_pointer (&self->data_raw, g_free);
  gstyle_clear_pointer (&self->data_raw_filtered, g_free);

  G_OBJECT_CLASS (gstyle_color_scale_parent_class)->finalize (object);
}

static void
gstyle_color_scale_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GstyleColorScale *self = GSTYLE_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, gstyle_color_scale_get_kind (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_scale_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GstyleColorScale *self = GSTYLE_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_KIND:
      gstyle_color_scale_set_kind (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gstyle_color_scale_class_init (GstyleColorScaleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gstyle_color_scale_finalize;
  object_class->get_property = gstyle_color_scale_get_property;
  object_class->set_property = gstyle_color_scale_set_property;
  widget_class->draw = gstyle_color_scale_draw;

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The kind of gradient used",
                       GSTYLE_TYPE_COLOR_SCALE_KIND,
                       GSTYLE_COLOR_SCALE_KIND_HUE,
                       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gstylecolorscale");
}

static void
free_color_stop (ColorStop *color_stop)
{
  g_slice_free (ColorStop, color_stop);
}

static void
gstyle_color_scale_init (GstyleColorScale *self)
{
  GtkStyleContext *context;

  gtk_widget_add_events (GTK_WIDGET (self), GDK_TOUCH_MASK);

  self->long_press_gesture = gtk_gesture_long_press_new (GTK_WIDGET (self));
  g_signal_connect (self->long_press_gesture, "pressed", G_CALLBACK (hold_action), self);

  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->long_press_gesture),
                                              GTK_PHASE_TARGET);

  self->custom_color_stops = g_sequence_new ((GDestroyNotify)free_color_stop);
  self->checkered_pattern = gstyle_utils_get_checkered_pattern ();

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  self->default_provider = gstyle_css_provider_init_default (gtk_style_context_get_screen (context));

  gtk_scale_set_draw_value (GTK_SCALE (self), FALSE);
}

GType
gstyle_color_scale_kind_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GSTYLE_COLOR_SCALE_KIND_HUE,          "GSTYLE_COLOR_SCALE_KIND_HUE",    "hue"    },
    { GSTYLE_COLOR_SCALE_KIND_GREY,         "GSTYLE_COLOR_SCALE_KIND_GREY",   "grey"   },
    { GSTYLE_COLOR_SCALE_KIND_ALPHA,        "GSTYLE_COLOR_SCALE_KIND_ALPHA",  "alpha"  },
    { GSTYLE_COLOR_SCALE_KIND_RED,          "GSTYLE_COLOR_SCALE_KIND_RED",    "red"    },
    { GSTYLE_COLOR_SCALE_KIND_GREEN,        "GSTYLE_COLOR_SCALE_KIND_GREEN",  "green"  },
    { GSTYLE_COLOR_SCALE_KIND_BLUE,         "GSTYLE_COLOR_SCALE_KIND_BLUE",   "blue"   },
    { GSTYLE_COLOR_SCALE_KIND_CUSTOM_STOPS, "GSTYLE_COLOR_SCALE_KIND_CUSTOM", "custom-stops" },
    { GSTYLE_COLOR_SCALE_KIND_CUSTOM_DATA,  "GSTYLE_COLOR_SCALE_KIND_CUSTOM", "custom-data" },
    { 0 }
  };

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;

      _type_id = g_enum_register_static ("GstyleColorScaleKind", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
