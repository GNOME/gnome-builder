/* rg-graph.c
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

#include "egg-signal-group.h"

#include "rg-graph.h"

typedef struct
{
  RgTable         *table;
  EggSignalGroup  *table_signals;
  GPtrArray       *renderers;
  cairo_surface_t *surface;
  guint            tick_handler;
  gdouble          x_offset;
  guint            surface_dirty : 1;
} RgGraphPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (RgGraph, rg_graph, GTK_TYPE_DRAWING_AREA)

enum {
  PROP_0,
  PROP_TABLE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkWidget *
rg_graph_new (void)
{
  return g_object_new (RG_TYPE_GRAPH, NULL);
}

static void
rg_graph_clear_surface (RgGraph *self)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_assert (RG_IS_GRAPH (self));

  priv->surface_dirty = TRUE;
}

/**
 * rg_graph_get_table:
 *
 * Gets the #RgGraph:table property.
 *
 * Returns: (transfer none) (nullable): An #RgTable or %NULL.
 */
RgTable *
rg_graph_get_table (RgGraph *self)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_return_val_if_fail (RG_IS_GRAPH (self), NULL);

  return priv->table;
}

void
rg_graph_set_table (RgGraph *self,
                    RgTable *table)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_return_if_fail (RG_IS_GRAPH (self));
  g_return_if_fail (!table || RG_IS_TABLE (table));

  if (g_set_object (&priv->table, table))
    {
      egg_signal_group_set_target (priv->table_signals, table);
      gtk_widget_queue_allocate (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TABLE]);
    }
}

void
rg_graph_add_renderer (RgGraph    *self,
                       RgRenderer *renderer)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_return_if_fail (RG_IS_GRAPH (self));
  g_return_if_fail (RG_IS_RENDERER (renderer));

  g_ptr_array_add (priv->renderers, g_object_ref (renderer));
  rg_graph_clear_surface (self);
}

static gboolean
rg_graph_tick_cb (GtkWidget     *widget,
                  GdkFrameClock *frame_clock,
                  gpointer       user_data)
{
  RgGraph *self = (RgGraph *)widget;
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);
  GtkAllocation alloc;
  gint64 frame_time;
  gint64 end_time;
  gint64 timespan;
  gdouble x_offset;

  g_assert (RG_IS_GRAPH (self));

  if ((priv->surface == NULL) || (priv->table == NULL) || !gtk_widget_get_visible (widget))
    goto remove_handler;

  timespan = rg_table_get_timespan (priv->table);
  if (timespan == 0)
    goto remove_handler;

  gtk_widget_get_allocation (widget, &alloc);

  frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  end_time = rg_table_get_end_time (priv->table);

  x_offset = -((frame_time - end_time) / (gdouble)timespan);

  if (x_offset != priv->x_offset)
    {
      priv->x_offset = x_offset;
      gtk_widget_queue_draw (widget);
    }

  return G_SOURCE_CONTINUE;

remove_handler:
  if (priv->tick_handler != 0)
    {
      gtk_widget_remove_tick_callback (widget, priv->tick_handler);
      priv->tick_handler = 0;
    }

  return G_SOURCE_REMOVE;
}

static void
rg_graph_ensure_surface (RgGraph *self)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);
  GtkAllocation alloc;
  RgTableIter iter;
  gint64 begin_time;
  gint64 end_time;
  gdouble y_begin;
  gdouble y_end;
  cairo_t *cr;
  gsize i;

  g_assert (RG_IS_GRAPH (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  if (priv->surface == NULL)
    {
      priv->surface_dirty = TRUE;
      priv->surface = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (self)),
                                                         CAIRO_CONTENT_COLOR_ALPHA,
                                                         alloc.width,
                                                         alloc.height);
    }

  if (priv->table == NULL)
    return;

  if (priv->surface_dirty)
    {
      priv->surface_dirty = FALSE;

      cr = cairo_create (priv->surface);

      cairo_save (cr);
      cairo_rectangle (cr, 0, 0, alloc.width, alloc.height);
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_fill (cr);
      cairo_restore (cr);

      g_object_get (priv->table,
                    "value-min", &y_begin,
                    "value-max", &y_end,
                    NULL);

      rg_table_get_iter_last (priv->table, &iter);
      end_time = rg_table_iter_get_timestamp (&iter);
      begin_time = end_time - rg_table_get_timespan (priv->table);

      for (i = 0; i < priv->renderers->len; i++)
        {
          RgRenderer *renderer;

          renderer = g_ptr_array_index (priv->renderers, i);

          cairo_save (cr);
          rg_renderer_render (renderer, priv->table, begin_time, end_time, y_begin, y_end, cr, &alloc);
          cairo_restore (cr);
        }

      cairo_destroy (cr);
    }

  if (priv->tick_handler == 0)
    priv->tick_handler = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                       rg_graph_tick_cb,
                                                       self,
                                                       NULL);
}

static gboolean
rg_graph_draw (GtkWidget *widget,
               cairo_t   *cr)
{
  RgGraph *self = (RgGraph *)widget;
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);
  GtkStyleContext *style_context;
  GtkAllocation alloc;

  g_assert (RG_IS_GRAPH (self));

  gtk_widget_get_allocation (widget, &alloc);

  style_context = gtk_widget_get_style_context (widget);

  rg_graph_ensure_surface (self);

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, "view");
  gtk_render_background (style_context, cr, 0, 0, alloc.width, alloc.height);
  gtk_style_context_restore (style_context);

  cairo_save (cr);
  cairo_set_source_surface (cr, priv->surface, priv->x_offset * alloc.width, 0);
  cairo_rectangle (cr, 0, 0, alloc.width, alloc.height);
  cairo_fill (cr);
  cairo_restore (cr);

  return GDK_EVENT_PROPAGATE;
}

static void
rg_graph_size_allocate (GtkWidget     *widget,
                        GtkAllocation *alloc)
{
  RgGraph *self = (RgGraph *)widget;
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);
  GtkAllocation old_alloc;

  g_assert (RG_IS_GRAPH (self));
  g_assert (alloc != NULL);

  gtk_widget_get_allocation (widget, &old_alloc);

  if ((old_alloc.width != alloc->width) || (old_alloc.height != alloc->height))
    g_clear_pointer (&priv->surface, cairo_surface_destroy);

  GTK_WIDGET_CLASS (rg_graph_parent_class)->size_allocate (widget, alloc);
}

static void
rg_graph__table_changed (RgGraph *self,
                         RgTable *table)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_assert (RG_IS_GRAPH (self));
  g_assert (RG_IS_TABLE (table));

  priv->x_offset = 0;

  rg_graph_clear_surface (self);
}

static void
rg_graph_destroy (GtkWidget *widget)
{
  RgGraph *self = (RgGraph *)widget;
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  if (priv->tick_handler != 0)
    {
      gtk_widget_remove_tick_callback (widget, priv->tick_handler);
      priv->tick_handler = 0;
    }

  GTK_WIDGET_CLASS (rg_graph_parent_class)->destroy (widget);
}

static void
rg_graph_finalize (GObject *object)
{
  RgGraph *self = (RgGraph *)object;
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  g_clear_object (&priv->table);
  g_clear_object (&priv->table_signals);
  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  g_clear_pointer (&priv->renderers, g_ptr_array_unref);

  G_OBJECT_CLASS (rg_graph_parent_class)->finalize (object);
}

static void
rg_graph_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  RgGraph *self = RG_GRAPH (object);

  switch (prop_id)
    {
    case PROP_TABLE:
      g_value_set_object (value, rg_graph_get_table (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_graph_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  RgGraph *self = RG_GRAPH (object);

  switch (prop_id)
    {
    case PROP_TABLE:
      rg_graph_set_table (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_graph_class_init (RgGraphClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = rg_graph_finalize;
  object_class->get_property = rg_graph_get_property;
  object_class->set_property = rg_graph_set_property;

  widget_class->destroy = rg_graph_destroy;
  widget_class->draw = rg_graph_draw;
  widget_class->size_allocate = rg_graph_size_allocate;

  properties [PROP_TABLE] =
    g_param_spec_object ("table",
                         "Table",
                         "The data table for the graph.",
                         RG_TYPE_TABLE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "rggraph");
}

static void
rg_graph_init (RgGraph *self)
{
  RgGraphPrivate *priv = rg_graph_get_instance_private (self);

  priv->renderers = g_ptr_array_new_with_free_func (g_object_unref);

  priv->table_signals = egg_signal_group_new (RG_TYPE_TABLE);

  egg_signal_group_connect_object (priv->table_signals,
                                   "notify::value-max",
                                   G_CALLBACK (gtk_widget_queue_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (priv->table_signals,
                                   "notify::value-min",
                                   G_CALLBACK (gtk_widget_queue_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (priv->table_signals,
                                   "notify::timespan",
                                   G_CALLBACK (gtk_widget_queue_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (priv->table_signals,
                                   "changed",
                                   G_CALLBACK (rg_graph__table_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
}
