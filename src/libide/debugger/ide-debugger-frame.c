/* ide-debugger-frame.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-frame"

#include "config.h"

#include "ide-debugger-frame.h"

typedef struct
{
  gchar              **args;
  gchar               *file;
  gchar               *function;
  gchar               *library;
  IdeDebuggerAddress   address;
  guint                depth;
  guint                line;
} IdeDebuggerFramePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerFrame, ide_debugger_frame, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_ARGS,
  PROP_DEPTH,
  PROP_FILE,
  PROP_FUNCTION,
  PROP_LIBRARY,
  PROP_LINE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_frame_finalize (GObject *object)
{
  IdeDebuggerFrame *self = (IdeDebuggerFrame *)object;
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_clear_pointer (&priv->args, g_strfreev);
  g_clear_pointer (&priv->file, g_free);
  g_clear_pointer (&priv->function, g_free);
  g_clear_pointer (&priv->library, g_free);

  G_OBJECT_CLASS (ide_debugger_frame_parent_class)->finalize (object);
}

static void
ide_debugger_frame_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeDebuggerFrame *self = IDE_DEBUGGER_FRAME (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_uint64 (value, ide_debugger_frame_get_address (self));
      break;

    case PROP_ARGS:
      g_value_set_boxed (value, ide_debugger_frame_get_args (self));
      break;

    case PROP_DEPTH:
      g_value_set_uint (value, ide_debugger_frame_get_depth (self));
      break;

    case PROP_FILE:
      g_value_set_string (value, ide_debugger_frame_get_file (self));
      break;

    case PROP_FUNCTION:
      g_value_set_string (value, ide_debugger_frame_get_function (self));
      break;

    case PROP_LIBRARY:
      g_value_set_string (value, ide_debugger_frame_get_library (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, ide_debugger_frame_get_line (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_frame_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeDebuggerFrame *self = IDE_DEBUGGER_FRAME (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      ide_debugger_frame_set_address (self, g_value_get_uint64 (value));
      break;

    case PROP_ARGS:
      ide_debugger_frame_set_args (self, g_value_get_boxed (value));
      break;

    case PROP_DEPTH:
      ide_debugger_frame_set_depth (self, g_value_get_uint (value));
      break;

    case PROP_FILE:
      ide_debugger_frame_set_file (self, g_value_get_string (value));
      break;

    case PROP_FUNCTION:
      ide_debugger_frame_set_function (self, g_value_get_string (value));
      break;

    case PROP_LIBRARY:
      ide_debugger_frame_set_library (self, g_value_get_string (value));
      break;

    case PROP_LINE:
      ide_debugger_frame_set_line (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_frame_class_init (IdeDebuggerFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_frame_finalize;
  object_class->get_property = ide_debugger_frame_get_property;
  object_class->set_property = ide_debugger_frame_set_property;

  properties [PROP_ADDRESS] =
    g_param_spec_uint64 ("address",
                         "Address",
                         "Address",
                         0,
                         G_MAXUINT64,
                         IDE_DEBUGGER_ADDRESS_INVALID,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ARGS] =
    g_param_spec_boxed ("args",
                        "Args",
                        "Args",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEPTH] =
    g_param_spec_uint ("depth",
                       "Depth",
                       "Depth",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE] =
    g_param_spec_string ("file",
                         "File",
                         "The file containing the frame location",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FUNCTION] =
    g_param_spec_string ("function",
                         "Function",
                         "The function the stack frame represents",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LIBRARY] =
    g_param_spec_string ("library",
                         "Library",
                         "The library containing the function, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LINE] =
    g_param_spec_uint ("line",
                         "Line",
                         "Line",
                         0, G_MAXUINT, 0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_frame_init (IdeDebuggerFrame *self)
{
}

IdeDebuggerFrame *
ide_debugger_frame_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_FRAME, NULL);
}

IdeDebuggerAddress
ide_debugger_frame_get_address (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), IDE_DEBUGGER_ADDRESS_INVALID);

  return priv->address;
}

void
ide_debugger_frame_set_address (IdeDebuggerFrame   *self,
                                IdeDebuggerAddress  address)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (priv->address != address)
    {
      priv->address = address;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ADDRESS]);
    }
}

const gchar * const *
ide_debugger_frame_get_args (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), NULL);

  return (const gchar * const *)priv->args;
}

void
ide_debugger_frame_set_args (IdeDebuggerFrame    *self,
                             const gchar * const *args)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (args != (const gchar * const *)priv->args)
    {
      g_strfreev (priv->args);
      priv->args = g_strdupv ((gchar **)args);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARGS]);
    }
}

const gchar *
ide_debugger_frame_get_file (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), NULL);

  return priv->file;
}

void
ide_debugger_frame_set_file (IdeDebuggerFrame *self,
                             const gchar      *file)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (g_set_str(&priv->file, file))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

const gchar *
ide_debugger_frame_get_function (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), NULL);

  return priv->function;
}

void
ide_debugger_frame_set_function (IdeDebuggerFrame *self,
                                 const gchar      *function)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (g_set_str (&priv->function, function))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FUNCTION]);
    }
}

const gchar *
ide_debugger_frame_get_library (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), NULL);

  return priv->library;
}

void
ide_debugger_frame_set_library (IdeDebuggerFrame *self,
                                const gchar      *library)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (g_set_str (&priv->library, library))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LIBRARY]);
    }
}

guint
ide_debugger_frame_get_line (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), 0);

  return priv->line;
}

void
ide_debugger_frame_set_line (IdeDebuggerFrame *self,
                             guint             line)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (priv->line != line)
    {
      priv->line = line;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LINE]);
    }
}

guint
ide_debugger_frame_get_depth (IdeDebuggerFrame *self)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_FRAME (self), 0);

  return priv->depth;
}

void
ide_debugger_frame_set_depth (IdeDebuggerFrame *self,
                              guint             depth)
{
  IdeDebuggerFramePrivate *priv = ide_debugger_frame_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (self));

  if (priv->depth != depth)
    {
      priv->depth = depth;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEPTH]);
    }
}
