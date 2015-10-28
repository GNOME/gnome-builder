/* rg-line-renderer.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <math.h>

#include "rg-line-renderer.h"

struct _RgLineRenderer
{
  GObject parent_instance;

  GdkRGBA stroke_color;
  gdouble line_width;
  guint   column;
};

static void rg_line_renderer_init_renderer (RgRendererInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RgLineRenderer, rg_line_renderer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (RG_TYPE_RENDERER,
                                                rg_line_renderer_init_renderer))

enum {
  PROP_0,
  PROP_COLUMN,
  PROP_LINE_WIDTH,
  PROP_STROKE_COLOR,
  PROP_STROKE_COLOR_RGBA,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

RgLineRenderer *
rg_line_renderer_new (void)
{
  return g_object_new (RG_TYPE_LINE_RENDERER, NULL);
}

static gdouble
calc_x (RgTableIter *iter,
        gint64       begin,
        gint64       end,
        guint        width)
{
  gint64 timestamp;

  timestamp = rg_table_iter_get_timestamp (iter);

  g_assert_cmpint (timestamp, !=, 0);

  return ((timestamp - begin) / (gdouble)(end - begin) * width);
}

static gdouble
calc_y (RgTableIter *iter,
        gdouble      range_begin,
        gdouble      range_end,
        guint        height,
        guint        column)
{
  GValue value = G_VALUE_INIT;
  gdouble y;

  rg_table_iter_get_value (iter, column, &value);

  switch (G_VALUE_TYPE (&value))
    {
    case G_TYPE_DOUBLE:
      y = g_value_get_double (&value);
      break;

    case G_TYPE_UINT:
      y = g_value_get_uint (&value);
      break;

    case G_TYPE_UINT64:
      y = g_value_get_uint64 (&value);
      break;

    case G_TYPE_INT:
      y = g_value_get_int (&value);
      break;

    case G_TYPE_INT64:
      y = g_value_get_int64 (&value);
      break;

    default:
      y = 0.0;
      break;
    }

  y -= range_begin;
  y /= (range_end - range_begin);
  y = height - (y * height);

  return y;
}

static void
rg_line_renderer_render (RgRenderer                  *renderer,
                         RgTable                     *table,
                         gint64                       x_begin,
                         gint64                       x_end,
                         gdouble                      y_begin,
                         gdouble                      y_end,
                         cairo_t                     *cr,
                         const cairo_rectangle_int_t *area)
{
  RgLineRenderer *self = (RgLineRenderer *)renderer;
  RgTableIter iter;

  g_assert (RG_IS_LINE_RENDERER (self));

  cairo_save (cr);

  if (rg_table_get_iter_first (table, &iter))
    {
      guint max_samples;
      gdouble chunk;
      gdouble last_x;
      gdouble last_y;

      max_samples = rg_table_get_max_samples (table);

      chunk = area->width / (gdouble)(max_samples - 1) / 2.0;

      last_x = calc_x (&iter, x_begin, x_end, area->width);
      last_y = calc_y (&iter, y_begin, y_end, area->height, self->column);

      cairo_move_to (cr, last_x, last_y);

      while (rg_table_iter_next (&iter))
        {
          gdouble x;
          gdouble y;

          x = calc_x (&iter, x_begin, x_end, area->width);
          y = calc_y (&iter, y_begin, y_end, area->height, self->column);

          cairo_curve_to (cr,
                          last_x + chunk,
                          last_y,
                          last_x + chunk,
                          y,
                          x,
                          y);

          last_x = x;
          last_y = y;
        }
    }

  cairo_set_line_width (cr, self->line_width);
  gdk_cairo_set_source_rgba (cr, &self->stroke_color);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static void
rg_line_renderer_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  RgLineRenderer *self = RG_LINE_RENDERER (object);

  switch (prop_id)
    {
    case PROP_COLUMN:
      g_value_set_uint (value, self->column);
      break;

    case PROP_LINE_WIDTH:
      g_value_set_double (value, self->line_width);
      break;

    case PROP_STROKE_COLOR:
      g_value_take_string (value, gdk_rgba_to_string (&self->stroke_color));
      break;

    case PROP_STROKE_COLOR_RGBA:
      g_value_set_boxed (value, &self->stroke_color);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_line_renderer_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  RgLineRenderer *self = RG_LINE_RENDERER (object);

  switch (prop_id)
    {
    case PROP_COLUMN:
      self->column = g_value_get_uint (value);
      break;

    case PROP_LINE_WIDTH:
      self->line_width = g_value_get_double (value);
      break;

    case PROP_STROKE_COLOR:
      rg_line_renderer_set_stroke_color (self, g_value_get_string (value));
      break;

    case PROP_STROKE_COLOR_RGBA:
      rg_line_renderer_set_stroke_color_rgba (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_line_renderer_class_init (RgLineRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = rg_line_renderer_get_property;
  object_class->set_property = rg_line_renderer_set_property;

  properties [PROP_COLUMN] =
    g_param_spec_uint ("column",
                       "Column",
                       "Column",
                       0, G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE_WIDTH] =
    g_param_spec_double ("line-width",
                         "Line Width",
                         "Line Width",
                         0.0, G_MAXDOUBLE,
                         1.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_STROKE_COLOR] =
    g_param_spec_string ("stroke-color",
                         "Stroke Color",
                         "Stroke Color",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_STROKE_COLOR_RGBA] =
    g_param_spec_boxed ("stroke-color-rgba",
                        "Stroke Color RGBA",
                        "Stroke Color RGBA",
                        GDK_TYPE_RGBA,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
rg_line_renderer_init (RgLineRenderer *self)
{
  self->line_width = 1.0;
}

static void
rg_line_renderer_init_renderer (RgRendererInterface *iface)
{
  iface->render = rg_line_renderer_render;
}

void
rg_line_renderer_set_stroke_color_rgba (RgLineRenderer *self,
                                        const GdkRGBA  *rgba)
{
  const GdkRGBA black = { 0, 0, 0, 1.0 };

  g_return_if_fail (RG_IS_LINE_RENDERER (self));

  if (rgba == NULL)
    rgba = &black;

  if (!gdk_rgba_equal (rgba, &self->stroke_color))
    {
      self->stroke_color = *rgba;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STROKE_COLOR_RGBA]);
    }
}

void
rg_line_renderer_set_stroke_color (RgLineRenderer *self,
                                   const gchar    *stroke_color)
{
  GdkRGBA rgba;

  g_return_if_fail (RG_IS_LINE_RENDERER (self));

  if (stroke_color == NULL)
    stroke_color = "#000000";

  if (gdk_rgba_parse (&rgba, stroke_color))
    rg_line_renderer_set_stroke_color_rgba (self, &rgba);
}
