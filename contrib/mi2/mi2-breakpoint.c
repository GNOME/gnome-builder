/* mi2-breakpoint.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "mi2-breakpoint"

#include "mi2-breakpoint.h"

struct _Mi2Breakpoint
{
  GObject  parent_instance;
  gchar   *address;
  gchar   *linespec;
  gchar   *filename;
  gchar   *function;
  gint     line_offset;
  gint     id;
};

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_FILENAME,
  PROP_FUNCTION,
  PROP_ID,
  PROP_LINE_OFFSET,
  PROP_LINESPEC,
  N_PROPS
};

G_DEFINE_TYPE (Mi2Breakpoint, mi2_breakpoint, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
mi2_breakpoint_finalize (GObject *object)
{
  Mi2Breakpoint *self = (Mi2Breakpoint *)object;

  g_clear_pointer (&self->address, g_free);
  g_clear_pointer (&self->filename, g_free);
  g_clear_pointer (&self->function, g_free);
  g_clear_pointer (&self->linespec, g_free);

  G_OBJECT_CLASS (mi2_breakpoint_parent_class)->finalize (object);
}

static void
mi2_breakpoint_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  Mi2Breakpoint *self = MI2_BREAKPOINT (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_string (value, mi2_breakpoint_get_address (self));
      break;

    case PROP_ID:
      g_value_set_int (value, mi2_breakpoint_get_id (self));
      break;

    case PROP_FILENAME:
      g_value_set_string (value, mi2_breakpoint_get_filename (self));
      break;

    case PROP_FUNCTION:
      g_value_set_string (value, mi2_breakpoint_get_function (self));
      break;

    case PROP_LINESPEC:
      g_value_set_string (value, mi2_breakpoint_get_linespec (self));
      break;

    case PROP_LINE_OFFSET:
      g_value_set_int (value, mi2_breakpoint_get_line_offset (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_breakpoint_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  Mi2Breakpoint *self = MI2_BREAKPOINT (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      mi2_breakpoint_set_address (self, g_value_get_string (value));
      break;

    case PROP_ID:
      mi2_breakpoint_set_id (self, g_value_get_int (value));
      break;

    case PROP_FILENAME:
      mi2_breakpoint_set_filename (self, g_value_get_string (value));
      break;

    case PROP_FUNCTION:
      mi2_breakpoint_set_function (self, g_value_get_string (value));
      break;

    case PROP_LINESPEC:
      mi2_breakpoint_set_linespec (self, g_value_get_string (value));
      break;

    case PROP_LINE_OFFSET:
      mi2_breakpoint_set_line_offset (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mi2_breakpoint_class_init (Mi2BreakpointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mi2_breakpoint_finalize;
  object_class->get_property = mi2_breakpoint_get_property;
  object_class->set_property = mi2_breakpoint_set_property;

  properties [PROP_ID] =
    g_param_spec_int ("id",
                      "Id",
                      "Id",
                      0, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ADDRESS] =
    g_param_spec_string ("address",
                         "Address",
                         "Address",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILENAME] =
    g_param_spec_string ("filename",
                         "Filename",
                         "Filename",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FUNCTION] =
    g_param_spec_string ("function",
                         "Function",
                         "Function",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINESPEC] =
    g_param_spec_string ("linespec",
                         "Linespec",
                         "Linespec",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE_OFFSET] =
    g_param_spec_int ("line-offset",
                      "Line Offset",
                      "The relative offset from the function or from the file",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mi2_breakpoint_init (Mi2Breakpoint *self)
{
}

gint
mi2_breakpoint_get_id (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), 0);

  return self->id;
}

void
mi2_breakpoint_set_id (Mi2Breakpoint *self,
                       gint           id)
{
  g_return_if_fail (MI2_IS_BREAKPOINT (self));
  g_return_if_fail (id >= 0);

  if (id != self->id)
    {
      self->id = id;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
    }
}

gint
mi2_breakpoint_get_line_offset (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), 0);

  return self->line_offset;
}

void
mi2_breakpoint_set_line_offset (Mi2Breakpoint *self,
                                gint           line_offset)
{
  g_return_if_fail (MI2_IS_BREAKPOINT (self));
  g_return_if_fail (line_offset >= 0);

  if (line_offset != self->line_offset)
    {
      self->line_offset = line_offset;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LINE_OFFSET]);
    }
}

const gchar *
mi2_breakpoint_get_filename (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), NULL);

  return self->filename;
}

void
mi2_breakpoint_set_filename (Mi2Breakpoint *self,
                             const gchar   *filename)
{
  g_return_if_fail (MI2_IS_BREAKPOINT (self));

  if (g_strcmp0 (self->filename, filename) != 0)
    {
      g_free (self->filename);
      self->filename = g_strdup (filename);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILENAME]);
    }
}

const gchar *
mi2_breakpoint_get_function (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), NULL);

  return self->function;
}

void
mi2_breakpoint_set_function (Mi2Breakpoint *self,
                             const gchar   *function)
{
  if (g_strcmp0 (self->function, function) != 0)
    {
      g_free (self->function);
      self->function = g_strdup (function);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FUNCTION]);
    }
}

const gchar *
mi2_breakpoint_get_linespec (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), NULL);

  return self->linespec;
}

void
mi2_breakpoint_set_linespec (Mi2Breakpoint *self,
                             const gchar   *linespec)
{
  if (g_strcmp0 (self->linespec, linespec) != 0)
    {
      g_free (self->linespec);
      self->linespec = g_strdup (linespec);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LINESPEC]);
    }
}

const gchar *
mi2_breakpoint_get_address (Mi2Breakpoint *self)
{
  g_return_val_if_fail (MI2_IS_BREAKPOINT (self), NULL);

  return self->address;
}

void
mi2_breakpoint_set_address (Mi2Breakpoint *self,
                            const gchar   *address)
{
  if (g_strcmp0 (self->address, address) != 0)
    {
      g_free (self->address);
      self->address = g_strdup (address);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ADDRESS]);
    }
}

Mi2Breakpoint *
mi2_breakpoint_new (void)
{
  return g_object_new (MI2_TYPE_BREAKPOINT, NULL);
}
