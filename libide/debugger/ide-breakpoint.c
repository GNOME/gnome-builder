/* ide-breakpoint.c
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

#define G_LOG_DOMAIN "ide-breakpoint"

#include "debugger/ide-breakpoint.h"

typedef struct
{
  gchar *id;
  GFile *file;
  guint  line;
  guint  enabled : 1;
} IdeBreakpointPrivate;

enum {
  PROP_0,
  PROP_ENABLED,
  PROP_FILE,
  PROP_ID,
  PROP_LINE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeBreakpoint, ide_breakpoint, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_breakpoint_finalize (GObject *object)
{
  IdeBreakpoint *self = (IdeBreakpoint *)object;
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_clear_object (&priv->file);
  g_clear_pointer (&priv->id, g_free);

  G_OBJECT_CLASS (ide_breakpoint_parent_class)->finalize (object);
}

static void
ide_breakpoint_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeBreakpoint *self = IDE_BREAKPOINT (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, ide_breakpoint_get_enabled (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_breakpoint_get_file (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_breakpoint_get_id (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, ide_breakpoint_get_line (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_breakpoint_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeBreakpoint *self = IDE_BREAKPOINT (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      ide_breakpoint_set_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_FILE:
      ide_breakpoint_set_file (self, g_value_get_object (value));
      break;

    case PROP_ID:
      ide_breakpoint_set_id (self, g_value_get_string (value));
      break;

    case PROP_LINE:
      ide_breakpoint_set_line (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_breakpoint_class_init (IdeBreakpointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_breakpoint_finalize;
  object_class->get_property = ide_breakpoint_get_property;
  object_class->set_property = ide_breakpoint_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Identifier for the breakpoint",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file for the breakpoint",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE] =
    g_param_spec_uint ("line",
                       "Line",
                       "The line number of the breakpoint",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If the breakpoint is enabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_breakpoint_init (IdeBreakpoint *self)
{
}

const gchar *
ide_breakpoint_get_id (IdeBreakpoint *self)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BREAKPOINT (self), NULL);

  return priv->id;
}

void
ide_breakpoint_set_id (IdeBreakpoint *self,
                       const gchar   *id)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_BREAKPOINT (self));

  if (g_strcmp0 (priv->id, id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

/**
 * ide_breakpoint_get_file:
 *
 * Gets the file containing the breakpoint, or %NULL.
 *
 * Returns: (transfer none) (nullable): A #GFile or %NULL.
 */
GFile *
ide_breakpoint_get_file (IdeBreakpoint *self)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BREAKPOINT (self), NULL);

  return priv->file;
}

void
ide_breakpoint_set_file (IdeBreakpoint *self,
                         GFile         *file)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_BREAKPOINT (self));

  if (g_set_object (&priv->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
}

guint
ide_breakpoint_get_line (IdeBreakpoint *self)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BREAKPOINT (self), 0);

  return priv->line;
}

void
ide_breakpoint_set_line (IdeBreakpoint *self,
                         guint          line)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_BREAKPOINT (self));

  if (priv->line != line)
    {
      priv->line = line;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LINE]);
    }
}

gboolean
ide_breakpoint_get_enabled (IdeBreakpoint *self)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BREAKPOINT (self), FALSE);

  return priv->enabled;
}

void
ide_breakpoint_set_enabled (IdeBreakpoint *self,
                            gboolean       enabled)
{
  IdeBreakpointPrivate *priv = ide_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_BREAKPOINT (self));

  enabled = !!enabled;

  if (priv->enabled != enabled)
    {
      priv->enabled = enabled;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}
