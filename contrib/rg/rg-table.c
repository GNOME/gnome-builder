/* rg-table.c
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

#include "rg-column-private.h"
#include "rg-table.h"

typedef struct
{
  GPtrArray *columns;
  RgColumn  *timestamps;

  guint      last_index;

  guint      max_samples;
  GTimeSpan  timespan;
  gdouble    value_max;
  gdouble    value_min;
} RgTablePrivate;

typedef struct
{
  RgTable *table;
  gint64   timestamp;
  guint    index;
} RgTableIterImpl;

enum {
  PROP_0,
  PROP_MAX_SAMPLES,
  PROP_TIMESPAN,
  PROP_VALUE_MAX,
  PROP_VALUE_MIN,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (RgTable, rg_table, G_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

gint64
rg_table_get_timespan (RgTable *self)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_val_if_fail (RG_IS_TABLE (self), 0);

  return priv->timespan;
}

void
rg_table_set_timespan (RgTable   *self,
                       GTimeSpan  timespan)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_if_fail (RG_IS_TABLE (self));

  if (timespan != priv->timespan)
    {
      priv->timespan = timespan;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TIMESPAN]);
    }
}

static void
rg_table_set_value_max (RgTable *self,
                        gdouble  value_max)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_if_fail (RG_IS_TABLE (self));

  if (priv->value_max != value_max)
    {
      priv->value_max = value_max;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE_MAX]);
    }
}

static void
rg_table_set_value_min (RgTable *self,
                        gdouble  value_min)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_if_fail (RG_IS_TABLE (self));

  if (priv->value_min != value_min)
    {
      priv->value_min = value_min;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE_MIN]);
    }
}

RgTable *
rg_table_new (void)
{
  return g_object_new (RG_TYPE_TABLE, NULL);
}

guint
rg_table_add_column (RgTable  *self,
                     RgColumn *column)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_val_if_fail (RG_IS_TABLE (self), 0);
  g_return_val_if_fail (RG_IS_COLUMN (column), 0);

  _rg_column_set_n_rows (column, priv->max_samples);

  g_ptr_array_add (priv->columns, g_object_ref (column));

  return priv->columns->len - 1;
}

guint
rg_table_get_max_samples (RgTable *self)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_return_val_if_fail (RG_IS_TABLE (self), 0);

  return priv->max_samples;
}

void
rg_table_set_max_samples (RgTable *self,
                          guint    max_samples)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);
  gsize i;

  g_return_if_fail (RG_IS_TABLE (self));
  g_return_if_fail (max_samples > 0);

  if (max_samples == priv->max_samples)
    return;

  for (i = 0; i < priv->columns->len; i++)
    {
      RgColumn *column;

      column = g_ptr_array_index (priv->columns, i);
      _rg_column_set_n_rows (column, max_samples);
    }

  _rg_column_set_n_rows (priv->timestamps, max_samples);

  priv->max_samples = max_samples;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MAX_SAMPLES]);
}

/**
 * rg_table_push:
 * @self: Table to push to
 * @iter: (out): Newly created #RgTableIter
 * @timestamp: Time of new event
 */
void
rg_table_push (RgTable     *self,
               RgTableIter *iter,
               gint64       timestamp)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;
  guint pos;
  gsize i;

  g_return_if_fail (RG_IS_TABLE (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (timestamp > 0);

  for (i = 0; i < priv->columns->len; i++)
    {
      RgColumn *column;

      column = g_ptr_array_index (priv->columns, i);
      _rg_column_push (column);
    }

  pos = _rg_column_push (priv->timestamps);
  _rg_column_set (priv->timestamps, pos, timestamp);

  impl->table = self;
  impl->timestamp = timestamp;
  impl->index = pos;

  priv->last_index = pos;

  g_signal_emit (self, signals [CHANGED], 0);
}

gboolean
rg_table_get_iter_last (RgTable     *self,
                        RgTableIter *iter)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;

  g_return_val_if_fail (RG_IS_TABLE (self), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (impl != NULL, FALSE);

  impl->table = self;
  impl->index = priv->last_index;
  impl->timestamp = 0;

  _rg_column_get (priv->timestamps, impl->index, &impl->timestamp);

  return (impl->timestamp != 0);
}

gint64
rg_table_get_end_time (RgTable *self)
{
  RgTableIter iter;

  g_return_val_if_fail (RG_IS_TABLE (self), 0);

  if (rg_table_get_iter_last (self, &iter))
    return rg_table_iter_get_timestamp (&iter);

  return g_get_monotonic_time ();
}

gboolean
rg_table_get_iter_first (RgTable     *self,
                         RgTableIter *iter)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;

  g_return_val_if_fail (RG_IS_TABLE (self), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (impl != NULL, FALSE);

  impl->table = self;
  impl->index = (priv->last_index + 1) % priv->max_samples;
  impl->timestamp = 0;

  _rg_column_get (priv->timestamps, impl->index, &impl->timestamp);

  /*
   * Maybe this is our first time around the ring, and we can just
   * assume the 0 index is the real first entry.
   */
  if (impl->timestamp == 0)
    {
      impl->index = 0;
      _rg_column_get (priv->timestamps, impl->index, &impl->timestamp);
    }

  return (impl->timestamp != 0);
}

gboolean
rg_table_iter_next (RgTableIter *iter)
{
  RgTablePrivate *priv;
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (impl != NULL, FALSE);
  g_return_val_if_fail (RG_IS_TABLE (impl->table), FALSE);

  priv = rg_table_get_instance_private (impl->table);

  if (impl->index == priv->last_index)
    {
      impl->table = NULL;
      impl->index = 0;
      impl->timestamp = 0;
      return FALSE;
    }

  do
    {
      impl->index = (impl->index + 1) % priv->max_samples;

      impl->timestamp = 0;
      _rg_column_get (priv->timestamps, impl->index, &impl->timestamp);

      if (impl->timestamp > 0)
        break;
    }
  while (impl->index < priv->last_index);

  return (impl->timestamp > 0);
}

gint64
rg_table_iter_get_timestamp (RgTableIter *iter)
{
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;

  g_return_val_if_fail (iter != NULL, 0);

  return impl->timestamp;
}

void
rg_table_iter_set (RgTableIter *iter,
                   gint         first_column,
                   ...)
{
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;
  RgTablePrivate *priv;
  gint column_id = first_column;
  va_list args;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (impl != NULL);
  g_return_if_fail (RG_IS_TABLE (impl->table));

  priv = rg_table_get_instance_private (impl->table);

  va_start (args, first_column);

  while (column_id >= 0)
    {
      RgColumn *column;

      if (column_id >= priv->columns->len)
        {
          g_critical ("No such column %d", column_id);
          goto cleanup;
        }

      column = g_ptr_array_index (priv->columns, column_id);

      _rg_column_collect (column, impl->index, args);

      column_id = va_arg (args, gint);
    }

  if (column_id != -1)
    g_critical ("Invalid column sentinal: %d", column_id);

cleanup:
  va_end (args);
}

void
rg_table_iter_get (RgTableIter *iter,
                   gint         first_column,
                   ...)
{
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;
  RgTablePrivate *priv;
  gint column_id = first_column;
  va_list args;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (impl != NULL);
  g_return_if_fail (RG_IS_TABLE (impl->table));

  priv = rg_table_get_instance_private (impl->table);

  va_start (args, first_column);

  while (column_id >= 0)
    {
      RgColumn *column;

      if (column_id >= priv->columns->len)
        {
          g_critical ("No such column %d", column_id);
          goto cleanup;
        }

      column = g_ptr_array_index (priv->columns, column_id);

      _rg_column_lcopy (column, impl->index, args);

      column_id = va_arg (args, gint);
    }

  if (column_id != -1)
    g_critical ("Invalid column sentinal: %d", column_id);

cleanup:
  va_end (args);
}

void
rg_table_iter_get_value (RgTableIter *iter,
                         guint        column,
                         GValue      *value)
{
  RgTableIterImpl *impl = (RgTableIterImpl *)iter;
  RgTablePrivate *priv;
  RgColumn *col;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (impl != NULL);
  g_return_if_fail (RG_IS_TABLE (impl->table));
  priv = rg_table_get_instance_private (impl->table);
  g_return_if_fail (column < priv->columns->len);

  col = g_ptr_array_index (priv->columns, column);
  _rg_column_get_value (col, impl->index, value);
}

static void
rg_table_finalize (GObject *object)
{
  RgTable *self = (RgTable *)object;
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  g_clear_pointer (&priv->columns, g_ptr_array_unref);

  G_OBJECT_CLASS (rg_table_parent_class)->finalize (object);
}

static void
rg_table_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  RgTable *self = (RgTable *)object;
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TIMESPAN:
      g_value_set_int64 (value, priv->timespan);
      break;

    case PROP_MAX_SAMPLES:
      g_value_set_uint (value, priv->max_samples);
      break;

    case PROP_VALUE_MAX:
      g_value_set_double (value, priv->value_max);
      break;

    case PROP_VALUE_MIN:
      g_value_set_double (value, priv->value_min);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_table_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  RgTable *self = (RgTable *)object;

  switch (prop_id)
    {
    case PROP_MAX_SAMPLES:
      rg_table_set_max_samples (self, g_value_get_uint (value));
      break;

    case PROP_TIMESPAN:
      rg_table_set_timespan (self, g_value_get_int64 (value));
      break;

    case PROP_VALUE_MAX:
      rg_table_set_value_max (self, g_value_get_double (value));
      break;

    case PROP_VALUE_MIN:
      rg_table_set_value_min (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_table_class_init (RgTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = rg_table_finalize;
  object_class->get_property = rg_table_get_property;
  object_class->set_property = rg_table_set_property;

  properties [PROP_MAX_SAMPLES] =
    g_param_spec_uint ("max-samples",
                       "Max Samples",
                       "Max Samples",
                       1, G_MAXUINT,
                       120,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_TIMESPAN] =
    g_param_spec_int64 ("timespan",
                        "Timespan",
                        "Timespan to visualize, in microseconds.",
                        1, G_MAXINT64,
                        G_USEC_PER_SEC * 60L,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE_MAX] =
    g_param_spec_double ("value-max",
                         "Value Max",
                         "Value Max",
                         -G_MINDOUBLE, G_MAXDOUBLE,
                         100.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE_MIN] =
    g_param_spec_double ("value-min",
                         "Value Min",
                         "Value Min",
                         -G_MINDOUBLE, G_MAXDOUBLE,
                         100.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [CHANGED] = g_signal_new ("changed",
                                     G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE,
                                     0);
}

static void
rg_table_init (RgTable *self)
{
  RgTablePrivate *priv = rg_table_get_instance_private (self);

  priv->max_samples = 60;
  priv->value_min = 0.0;
  priv->value_max = 100.0;

  priv->columns = g_ptr_array_new_with_free_func (g_object_unref);

  priv->timestamps = rg_column_new (NULL, G_TYPE_INT64);
  _rg_column_set_n_rows (priv->timestamps, priv->max_samples);
}
