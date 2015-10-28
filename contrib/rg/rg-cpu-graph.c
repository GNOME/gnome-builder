/* rg-cpu-graph.c
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

#include "rg-cpu-graph.h"
#include "rg-cpu-table.h"
#include "rg-line-renderer.h"

struct _RgCpuGraph
{
  RgGraph parent_instance;

  gint64 timespan;
  guint  max_samples;
};

G_DEFINE_TYPE (RgCpuGraph, rg_cpu_graph, RG_TYPE_GRAPH)

enum {
  PROP_0,
  PROP_MAX_SAMPLES,
  PROP_TIMESPAN,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static gchar *colors[] = {
  "#73d216",
  "#f57900",
  "#3465a4",
  "#ef2929",
  "#75507b",
  "#ce5c00",
  "#c17d11",
  "#ce5c00",
};

GtkWidget *
rg_cpu_graph_new (void)
{
  return g_object_new (RG_TYPE_CPU_GRAPH, NULL);
}

static void
rg_cpu_graph_constructed (GObject *object)
{
  static RgCpuTable *table;
  RgCpuGraph *self = (RgCpuGraph *)object;
  guint n_cpu;
  guint i;

  G_OBJECT_CLASS (rg_cpu_graph_parent_class)->constructed (object);

  /*
   * Create a table, but allow it to be destroyed after the last
   * graph releases it. We will recreate it on demand.
   */
  if (table == NULL)
    {
      table = g_object_new (RG_TYPE_CPU_TABLE,
                            "timespan", self->timespan,
                            "max-samples", self->max_samples + 1,
                            NULL);
      g_object_add_weak_pointer (G_OBJECT (table), (gpointer *)&table);
      rg_graph_set_table (RG_GRAPH (self), RG_TABLE (table));
      g_object_unref (table);
    }
  else
    {
      rg_graph_set_table (RG_GRAPH (self), RG_TABLE (table));
    }

  n_cpu = g_get_num_processors ();

  for (i = 0; i < n_cpu; i++)
    {
      RgRenderer *renderer;

      renderer = g_object_new (RG_TYPE_LINE_RENDERER,
                               "column", i,
                               "stroke-color", colors [i % G_N_ELEMENTS (colors)],
                               NULL);
      rg_graph_add_renderer (RG_GRAPH (self), renderer);
      g_clear_object (&renderer);
    }
}

static void
rg_cpu_graph_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  RgCpuGraph *self = RG_CPU_GRAPH (object);

  switch (prop_id)
    {
    case PROP_MAX_SAMPLES:
      g_value_set_uint (value, self->max_samples);
      break;

    case PROP_TIMESPAN:
      g_value_set_int64 (value, self->timespan);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_cpu_graph_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  RgCpuGraph *self = RG_CPU_GRAPH (object);

  switch (prop_id)
    {
    case PROP_MAX_SAMPLES:
      self->max_samples = g_value_get_uint (value);
      break;

    case PROP_TIMESPAN:
      self->timespan = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_cpu_graph_class_init (RgCpuGraphClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = rg_cpu_graph_constructed;
  object_class->get_property = rg_cpu_graph_get_property;
  object_class->set_property = rg_cpu_graph_set_property;

  properties [PROP_TIMESPAN] =
    g_param_spec_int64 ("timespan",
                         "Timespan",
                         "Timespan",
                         0, G_MAXINT64,
                         0,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MAX_SAMPLES] =
    g_param_spec_uint ("max-samples",
                       "Max Samples",
                       "Max Samples",
                       0, G_MAXUINT,
                       120,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
rg_cpu_graph_init (RgCpuGraph *self)
{
  self->max_samples = 120;
  self->timespan = 60L * G_USEC_PER_SEC;
}
