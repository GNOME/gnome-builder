/* ide-debugger-breakpoint.c
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

#define G_LOG_DOMAIN "ide-debugger-breakpoint"

#include "config.h"

#include "ide-debugger-breakpoint.h"
#include "ide-debugger-private.h"
#include "ide-debugger-types.h"

typedef struct
{
  gchar                  *function;
  gchar                  *id;
  gchar                  *file;
  gchar                  *spec;
  gchar                  *thread;

  guint                   line;

  IdeDebuggerDisposition  disposition : 8;
  IdeDebuggerBreakMode    mode : 8;
  guint                   enabled : 1;

  IdeDebuggerAddress      address;
  gint64                  count;
} IdeDebuggerBreakpointPrivate;

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_COUNT,
  PROP_DISPOSITION,
  PROP_ENABLED,
  PROP_FILE,
  PROP_FUNCTION,
  PROP_ID,
  PROP_LINE,
  PROP_MODE,
  PROP_SPEC,
  PROP_THREAD,
  N_PROPS
};

enum {
  RESET,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerBreakpoint, ide_debugger_breakpoint, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_debugger_breakpoint_real_reset (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (self));

  g_clear_pointer (&priv->id, g_free);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
}

static void
ide_debugger_breakpoint_finalize (GObject *object)
{
  IdeDebuggerBreakpoint *self = (IdeDebuggerBreakpoint *)object;
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_clear_pointer (&priv->function, g_free);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->file, g_free);
  g_clear_pointer (&priv->spec, g_free);
  g_clear_pointer (&priv->thread, g_free);

  G_OBJECT_CLASS (ide_debugger_breakpoint_parent_class)->finalize (object);
}

static void
ide_debugger_breakpoint_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeDebuggerBreakpoint *self = IDE_DEBUGGER_BREAKPOINT (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_uint64 (value, ide_debugger_breakpoint_get_address (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_debugger_breakpoint_get_id (self));
      break;

    case PROP_COUNT:
      g_value_set_int64 (value, ide_debugger_breakpoint_get_count (self));
      break;

    case PROP_DISPOSITION:
      g_value_set_enum (value, ide_debugger_breakpoint_get_disposition (self));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, ide_debugger_breakpoint_get_enabled (self));
      break;

    case PROP_FILE:
      g_value_set_string (value, ide_debugger_breakpoint_get_file (self));
      break;

    case PROP_FUNCTION:
      g_value_set_string (value, ide_debugger_breakpoint_get_function (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, ide_debugger_breakpoint_get_line (self));
      break;

    case PROP_MODE:
      g_value_set_enum (value, ide_debugger_breakpoint_get_mode (self));
      break;

    case PROP_SPEC:
      g_value_set_string (value, ide_debugger_breakpoint_get_spec (self));
      break;

    case PROP_THREAD:
      g_value_set_string (value, ide_debugger_breakpoint_get_thread (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoint_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeDebuggerBreakpoint *self = IDE_DEBUGGER_BREAKPOINT (object);
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      ide_debugger_breakpoint_set_address (self, g_value_get_uint64 (value));
      break;

    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_COUNT:
      ide_debugger_breakpoint_set_count (self, g_value_get_int64 (value));
      break;

    case PROP_DISPOSITION:
      ide_debugger_breakpoint_set_disposition (self, g_value_get_enum (value));
      break;

    case PROP_ENABLED:
      ide_debugger_breakpoint_set_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_FILE:
      ide_debugger_breakpoint_set_file (self, g_value_get_string (value));
      break;

    case PROP_FUNCTION:
      ide_debugger_breakpoint_set_function (self, g_value_get_string (value));
      break;

    case PROP_LINE:
      ide_debugger_breakpoint_set_line (self, g_value_get_uint (value));
      break;

    case PROP_MODE:
      ide_debugger_breakpoint_set_mode (self, g_value_get_enum (value));
      break;

    case PROP_SPEC:
      ide_debugger_breakpoint_set_spec (self, g_value_get_string (value));
      break;

    case PROP_THREAD:
      ide_debugger_breakpoint_set_thread (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoint_class_init (IdeDebuggerBreakpointClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_breakpoint_finalize;
  object_class->get_property = ide_debugger_breakpoint_get_property;
  object_class->set_property = ide_debugger_breakpoint_set_property;

  klass->reset = ide_debugger_breakpoint_real_reset;

  /**
   * IdeDebuggerBreakpoint:address:
   *
   * The address of the breakpoint, if available.
   *
   * Builder only supports up to 64-bit addresses at this time.
   */
  properties [PROP_ADDRESS] =
    g_param_spec_uint64 ("address",
                         "Address",
                         "The address of the breakpoint",
                         0, G_MAXUINT64, IDE_DEBUGGER_ADDRESS_INVALID,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:count:
   *
   * The number of times the breakpoint has been reached.
   *
   * This is backend specific, and may not be supported by all backends.
   */
  properties [PROP_COUNT] =
    g_param_spec_int64 ("count",
                        "Count",
                        "The number of times the breakpoint has hit",
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:disposition:
   *
   * This property describes what should happen to the breakpoint upon the
   * next stop of the debugger.
   *
   * Generally, breakpoints are kept. But some backends allow you to remove
   * a breakpoint upon the next stop of the debugger or when the breakpoint
   * is next reached.
   *
   * This is backend specific, and not all values may be supported by all
   * backends.
   */
  properties [PROP_DISPOSITION] =
    g_param_spec_enum ("disposition",
                         "Disposition",
                         "The disposition of the breakpoint",
                         IDE_TYPE_DEBUGGER_DISPOSITION,
                         IDE_DEBUGGER_DISPOSITION_KEEP,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:enabled:
   *
   * This property is %TRUE when the breakpoint is enabled.
   */
  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "Enabled",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:function:
   *
   * The name of the function containing the breakpoint.
   *
   * The value of this is backend specific and may look vastly different
   * based on the language being debugged.
   */
  properties [PROP_FUNCTION] =
    g_param_spec_string ("function",
                         "Function",
                         "Function",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:id:
   *
   * The identifier of the breakpoint.
   *
   * This is backend specific.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Identifier",
                         "The identifier for the breakpoint",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:file:
   *
   * The file containing the breakpoint, if any.
   *
   * If the breakpoint exists at an assembly instruction that cannot be
   * represented by a file, this will be %NULL.
   */
  properties [PROP_FILE] =
    g_param_spec_string ("file",
                         "File",
                         "The file containing the breakpoint",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:line:
   *
   * The line number within #IdeDebuggerBreakpoint:file where the
   * breakpoint exists.
   */
  properties [PROP_LINE] =
    g_param_spec_uint ("line",
                       "Line",
                       "Line",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:mode:
   *
   * The mode of the breakpoint, such as a breakpoint, countpoint, or watchpoint.
   */
  properties [PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Mode",
                       "The breakpoint mode",
                       IDE_TYPE_DEBUGGER_BREAK_MODE,
                       IDE_DEBUGGER_BREAK_BREAKPOINT,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:spec:
   *
   * The specification for the breakpoint, which may be used by watchpoints
   * to determine of the breakpoint should be applied while executing.
   */
  properties [PROP_SPEC] =
    g_param_spec_string ("spec",
                         "Spec",
                         "The specification for a data breakpoint",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerBreakpoint:thread:
   *
   * The thread the breakpoint is currently stopped in, or %NULL.
   */
  properties [PROP_THREAD] =
    g_param_spec_string ("thread",
                         "Thread",
                         "Thread",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeDebuggerBreakpoint::reset:
   * @self: An #IdeDebuggerBreakpoint
   *
   * The "reset" signal is emitted after the debugger has exited so that the
   * breakpoint can reset any internal state. This allows the breakpoint to be
   * propagated to the next debugger instance, allowing the user to move
   * between debugger sessions without loosing state.
   */
  signals [RESET] =
    g_signal_new ("reset",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerBreakpointClass, reset),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
ide_debugger_breakpoint_init (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  priv->disposition = IDE_DEBUGGER_DISPOSITION_KEEP;
  priv->enabled = TRUE;
  priv->mode = IDE_DEBUGGER_BREAK_BREAKPOINT;
}

IdeDebuggerBreakpoint *
ide_debugger_breakpoint_new (const gchar *id)
{
  return g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINT,
                       "id", id,
                       NULL);
}

/**
 * ide_debugger_breakpoint_get_id:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the identifier for the breakpoint that is backend specific.
 *
 * Returns: the id of the breakpoint
 */
const gchar *
ide_debugger_breakpoint_get_id (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), NULL);

  return priv->id;
}

/**
 * ide_debugger_breakpoint_get_address:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "address" property, which defines where the breakpoint is
 * located in memory.
 *
 * Builder only supports up to 64-bit addresses at this time.
 *
 * Returns: The address of the breakpoint, if any.
 */
IdeDebuggerAddress
ide_debugger_breakpoint_get_address (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), IDE_DEBUGGER_ADDRESS_INVALID);

  return priv->address;
}

/**
 * ide_debugger_breakpoint_set_address:
 * @self: An #IdeDebuggerBreakpoint
 * @address: The address of the breakpoint
 *
 * Sets the address of the breakpoint, if any.
 */
void
ide_debugger_breakpoint_set_address (IdeDebuggerBreakpoint *self,
                                     IdeDebuggerAddress     address)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (priv->address != address)
    {
      priv->address = address;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ADDRESS]);
    }
}

/**
 * ide_debugger_breakpoint_get_file:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the file that contains the breakpoint. This may be %NULL, particularly
 * if the breakpoint does not exist with in a known file, such as at a memory
 * address.
 *
 * Returns: (nullable): The file containing the breakpoint, or %NULL
 */
const gchar *
ide_debugger_breakpoint_get_file (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), NULL);

  return priv->file;
}

/**
 * ide_debugger_breakpoint_set_file:
 * @self: An #IdeDebuggerBreakpoint
 * @file: (nullable): the file containing the breakpoint, or %NULL
 *
 * Sets the file that contains the breakpoint, if any.
 */
void
ide_debugger_breakpoint_set_file (IdeDebuggerBreakpoint *self,
                                  const gchar           *file)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (g_set_str (&priv->file, file))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

/**
 * ide_debugger_breakpoint_get_spec:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "spec" property of the breakpoint.
 *
 * The spec is used when the #IdeDebuggerBreakMode is
 * %IDE_DEBUGGER_BREAK_WATCHPOINT.
 *
 * Returns: (nullable): A string containing the spec, or %NULL
 */
const gchar *
ide_debugger_breakpoint_get_spec (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), NULL);

  return priv->spec;
}

/**
 * ide_debugger_breakpoint_set_spec:
 * @self: An #IdeDebuggerBreakpoint
 * @spec: (nullable): the specification or %NULL
 *
 * Sets the specification for the debugger breakpoint. This describes
 * a statement which the debugger can use to determine of the breakpoint
 * should be applied when stopping the debugger.
 */
void
ide_debugger_breakpoint_set_spec (IdeDebuggerBreakpoint *self,
                                  const gchar           *spec)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (g_set_str (&priv->spec, spec))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SPEC]);
    }
}

/**
 * ide_debugger_breakpoint_get_count:
 *
 * Gets the number of times the breakpoint has been reached, if supported
 * by the debugger backend.
 *
 * Returns: An integer greater than or equal to zero representing the
 *   number of times the breakpoint has been reached.
 */
gint64
ide_debugger_breakpoint_get_count (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), 0);

  return priv->count;
}

/**
 * ide_debugger_breakpoint_set_count:
 *
 * Sets the number of times the breakpoint has been reached if the
 * breakpoint is a countpoint (or if the backend supports counting of
 * regular breakpoints).
 */
void
ide_debugger_breakpoint_set_count (IdeDebuggerBreakpoint *self,
                                   gint64                 count)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (priv->count != count)
    {
      priv->count = count;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COUNT]);
    }
}

/**
 * ide_debugger_breakpoint_get_mode:
 *
 * Gets teh mode for the breakpoint. This describes if the breakpoint
 * is a normal breakpoint type, countpoint, or watchpoint.
 *
 * See also: #IdeDebuggerBreakMode
 *
 * Returns: The mode of the breakpoint
 */
IdeDebuggerBreakMode
ide_debugger_breakpoint_get_mode (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), 0);

  return priv->mode;
}

/**
 * ide_debugger_breakpoint_set_mode:
 * @self: An #IdeDebuggerBreakpoint
 * @mode: An #IdeDebuggerBreakMode
 *
 * Sets the "mode" property for the breakpoint.
 *
 * This should represent the mode for which the breakpoint is used.
 *
 * For example, if it is a countpoint (a breakpoint which increments a
 * counter), you would use %IDE_DEBUGGER_BREAK_COUNTPOINT.
 */
void
ide_debugger_breakpoint_set_mode (IdeDebuggerBreakpoint *self,
                                  IdeDebuggerBreakMode   mode)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAK_MODE (mode));

  if (priv->mode != mode)
    {
      priv->mode = mode;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODE]);
    }
}

/**
 * ide_debugger_breakpoint_get_disposition:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "disposition" property of the breakpoint.
 *
 * Returns: An #IdeDebugerDisposition
 */
IdeDebuggerDisposition
ide_debugger_breakpoint_get_disposition (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), 0);

  return priv->disposition;
}

/**
 * ide_debugger_breakpoint_set_disposition:
 * @self: an #IdeDebuggerBreakpoint
 * @disposition: an #IdeDebuggerDisposition
 *
 * Sets the "disposition" property.
 *
 * The disposition property is used to to track what should happen to a
 * breakpoint when movements are made in the debugger.
 */
void
ide_debugger_breakpoint_set_disposition (IdeDebuggerBreakpoint  *self,
                                         IdeDebuggerDisposition  disposition)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));
  g_return_if_fail (IDE_IS_DEBUGGER_DISPOSITION (disposition));

  if (disposition != priv->disposition)
    {
      priv->disposition = disposition;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPOSITION]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

/**
 * ide_debugger_breakpoint_get_enabled:
 *
 * Checks if the breakpoint is enabled.
 *
 * Returns: %TRUE if the breakpoint is enabled
 */
gboolean
ide_debugger_breakpoint_get_enabled (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), FALSE);

  return priv->enabled;
}

/**
 * ide_debugger_breakpoint_set_enabled:
 * @self: a #IdeDebuggerBreakpoint
 * @enabled: if the breakpoint is enabled
 *
 * Sets the enabled state of the breakpoint instance.
 *
 * You must call ide_debugger_breakpoint_modify_breakpoint_async() to actually
 * modify the breakpoint in the backend.
 */
void
ide_debugger_breakpoint_set_enabled (IdeDebuggerBreakpoint *self,
                                     gboolean               enabled)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  enabled = !!enabled;

  if (priv->enabled != enabled)
    {
      priv->enabled = enabled;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

/**
 * ide_debugger_breakpoint_get_function:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "function" property of the breakpoint.
 *
 * This is a user-readable value representing the name of the function.
 */
const gchar *
ide_debugger_breakpoint_get_function (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), NULL);

  return priv->function;
}

/**
 * ide_debugger_breakpoint_set_function:
 * @self: An #IdeDebuggerBreakpoint
 * @function: (nullable): the name of the function, or %NULL
 *
 * Sets the "function" property, which is a user-readable value representing
 * the name of the function.
 */
void
ide_debugger_breakpoint_set_function (IdeDebuggerBreakpoint *self,
                                      const gchar           *function)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (g_set_str (&priv->function, function))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FUNCTION]);
    }
}

/**
 * ide_debugger_breakpoint_get_line:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "line" property, which is the line number within the file
 * that contains the breakpoint.
 *
 * This value is indexed from 1, and 0 indicates that the value is unset.
 *
 * Returns: An integer greater than 0 if set, otherwise 0.
 */
guint
ide_debugger_breakpoint_get_line (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), 0);

  return priv->line;
}

/**
 * ide_debugger_breakpoint_set_line:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Sets the line for the breakpoint. A value of 0 means the line is unset.
 */
void
ide_debugger_breakpoint_set_line (IdeDebuggerBreakpoint *self,
                                  guint                  line)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (priv->line != line)
    {
      priv->line = line;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LINE]);
    }
}

/**
 * ide_debugger_breakpoint_get_thread:
 * @self: An #IdeDebuggerBreakpoint
 *
 * Gets the "thread" property, which is the thread the breakpoint is
 * currently stopped in (if any).
 *
 * Returns: (nullable): the thread identifier or %NULL
 */
const gchar *
ide_debugger_breakpoint_get_thread (IdeDebuggerBreakpoint *self)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self), NULL);

  return priv->thread;
}

/**
 * ide_debugger_breakpoint_set_thread:
 *
 * Sets the thread that the breakpoint is currently stopped in.
 *
 * This should generally only be used by debugger implementations.
 */
void
ide_debugger_breakpoint_set_thread (IdeDebuggerBreakpoint *self,
                                    const gchar           *thread)
{
  IdeDebuggerBreakpointPrivate *priv = ide_debugger_breakpoint_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  if (g_set_str (&priv->thread, thread))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_THREAD]);
    }
}

gint
ide_debugger_breakpoint_compare (IdeDebuggerBreakpoint *a,
                                 IdeDebuggerBreakpoint *b)
{
  IdeDebuggerBreakpointPrivate *priv_a = ide_debugger_breakpoint_get_instance_private (a);
  IdeDebuggerBreakpointPrivate *priv_b = ide_debugger_breakpoint_get_instance_private (b);

  if (a == b)
    return 0;

  /* Rely on pointer comparison for breakpoints that
   * don't yet have an identifier.
   */
  if (priv_a->id == NULL && priv_b->id == NULL)
    return a - b;

  if (priv_a->id && priv_b->id)
    {
      if (g_ascii_isdigit (*priv_a->id) && g_ascii_isdigit (*priv_b->id))
        return g_ascii_strtoll (priv_a->id, NULL, 10) -
               g_ascii_strtoll (priv_b->id, NULL, 10);
    }

  return g_strcmp0 (priv_a->id, priv_b->id);
}

void
_ide_debugger_breakpoint_reset (IdeDebuggerBreakpoint *self)
{
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (self));

  g_signal_emit (self, signals [RESET], 0);
}
