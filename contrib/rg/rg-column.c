/* rg-column.c
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
#include <gobject/gvaluecollector.h>

#include "rg-column.h"
#include "rg-column-private.h"
#include "rg-ring.h"

struct _RgColumn
{
  GObject  parent_instance;
  gchar   *name;
  RgRing  *values;
  GType    value_type;
};

G_DEFINE_TYPE (RgColumn, rg_column, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NAME,
  PROP_VALUE_TYPE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

RgColumn *
rg_column_new (const gchar *name,
               GType        value_type)
{
  return g_object_new (RG_TYPE_COLUMN,
                       "name", name,
                       "value-type", value_type,
                       NULL);
}

const gchar *
rg_column_get_name (RgColumn *self)
{
  g_return_val_if_fail (RG_IS_COLUMN (self), NULL);

  return self->name;
}

void
rg_column_set_name (RgColumn    *self,
                    const gchar *name)
{
  g_return_if_fail (RG_IS_COLUMN (self));

  if (g_strcmp0 (name, self->name) != 0)
    {
      g_free (self->name);
      self->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

static void
rg_column_copy_value (gpointer data,
                      gpointer user_data)
{
  const GValue *src_value = data;
  RgRing *ring = user_data;
  GValue copy = G_VALUE_INIT;

  if (G_IS_VALUE (src_value))
    {
      g_value_init (&copy, G_VALUE_TYPE (src_value));
      g_value_copy (src_value, &copy);
    }

  rg_ring_append_val (ring, copy);
}

void
_rg_column_set_n_rows (RgColumn *self,
                       guint     n_rows)
{
  RgRing *ring;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (n_rows > 0);

  ring = rg_ring_sized_new (sizeof (GValue), n_rows, NULL);
  rg_ring_foreach (self->values, rg_column_copy_value, ring);
  g_clear_pointer (&self->values, rg_ring_unref);
  self->values = ring;
}

guint
_rg_column_push (RgColumn *self)
{
  GValue value = G_VALUE_INIT;
  guint ret;

  g_return_val_if_fail (RG_IS_COLUMN (self), 0);

  g_value_init (&value, self->value_type);
  ret = rg_ring_append_val (self->values, value);

  return ret;
}

void
_rg_column_get_value (RgColumn *self,
                      guint     index,
                      GValue   *value)
{
  const GValue *src_value;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (value != NULL);
  g_return_if_fail (index < self->values->len);

  src_value = &((GValue *)(gpointer)self->values->data)[index];

  g_value_init (value, self->value_type);
  if (G_IS_VALUE (src_value))
    g_value_copy (src_value, value);
}

void
_rg_column_collect (RgColumn *self,
                    guint     index,
                    va_list   args)
{
  GValue *value;
  gchar *errmsg = NULL;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (index < self->values->len);

  value = &((GValue *)(gpointer)self->values->data)[index];

  G_VALUE_COLLECT (value, args, 0, &errmsg);

  if (G_UNLIKELY (errmsg != NULL))
    {
      g_critical ("%s", errmsg);
      g_free (errmsg);
    }
}

void
_rg_column_set (RgColumn *self,
                guint     index,
                ...)
{
  va_list args;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (index < self->values->len);

  va_start (args, index);
  _rg_column_collect (self, index, args);
  va_end (args);
}

void
_rg_column_get (RgColumn *self,
                guint     index,
                ...)
{
  va_list args;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (index < self->values->len);

  va_start (args, index);
  _rg_column_lcopy (self, index, args);
  va_end (args);
}

void
_rg_column_lcopy (RgColumn *self,
                  guint     index,
                  va_list   args)
{
  const GValue *value;
  gchar *errmsg = NULL;

  g_return_if_fail (RG_IS_COLUMN (self));
  g_return_if_fail (index < self->values->len);

  value = &((GValue *)(gpointer)self->values->data)[index];

  if (!G_IS_VALUE (value))
    return;

  G_VALUE_LCOPY (value, args, 0, &errmsg);

  if (G_UNLIKELY (errmsg != NULL))
    {
      g_critical ("%s", errmsg);
      g_free (errmsg);
    }
}

static void
rg_column_finalize (GObject *object)
{
  RgColumn *self = (RgColumn *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->values, rg_ring_unref);

  G_OBJECT_CLASS (rg_column_parent_class)->finalize (object);
}

static void
rg_column_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  RgColumn *self = RG_COLUMN (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, rg_column_get_name (self));
      break;

    case PROP_VALUE_TYPE:
      g_value_set_gtype (value, self->value_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_column_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  RgColumn *self = RG_COLUMN (object);

  switch (prop_id)
    {
    case PROP_NAME:
      rg_column_set_name (self, g_value_get_string (value));
      break;

    case PROP_VALUE_TYPE:
      self->value_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rg_column_class_init (RgColumnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = rg_column_finalize;
  object_class->get_property = rg_column_get_property;
  object_class->set_property = rg_column_set_property;

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the column",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE_TYPE] =
    g_param_spec_gtype ("value-type",
                        "Value Type",
                        "Value Type",
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
rg_column_init (RgColumn *self)
{
  self->values = rg_ring_sized_new (sizeof (GValue), 60, (GDestroyNotify)g_value_unset);
}
