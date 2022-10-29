/* ide-debugger-instruction.c
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

#define G_LOG_DOMAIN "ide-debugger-instruction"

#include "config.h"

#include "ide-debugger-instruction.h"

typedef struct
{
  IdeDebuggerAddress  address;
  gchar              *display;
  gchar              *function;
} IdeDebuggerInstructionPrivate;

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_DISPLAY,
  PROP_FUNCTION,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerInstruction, ide_debugger_instruction, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_instruction_finalize (GObject *object)
{
  IdeDebuggerInstruction *self = (IdeDebuggerInstruction *)object;
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_clear_pointer (&priv->display, g_free);
  g_clear_pointer (&priv->function, g_free);

  G_OBJECT_CLASS (ide_debugger_instruction_parent_class)->finalize (object);
}

static void
ide_debugger_instruction_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeDebuggerInstruction *self = IDE_DEBUGGER_INSTRUCTION (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      g_value_set_uint64 (value, ide_debugger_instruction_get_address (self));
      break;

    case PROP_DISPLAY:
      g_value_set_string (value, ide_debugger_instruction_get_display (self));
      break;

    case PROP_FUNCTION:
      g_value_set_string (value, ide_debugger_instruction_get_function (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_instruction_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeDebuggerInstruction *self = IDE_DEBUGGER_INSTRUCTION (object);
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      priv->address = g_value_get_uint64 (value);
      break;

    case PROP_DISPLAY:
      ide_debugger_instruction_set_display (self, g_value_get_string (value));
      break;

    case PROP_FUNCTION:
      ide_debugger_instruction_set_function (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_instruction_class_init (IdeDebuggerInstructionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_instruction_finalize;
  object_class->get_property = ide_debugger_instruction_get_property;
  object_class->set_property = ide_debugger_instruction_set_property;

  properties [PROP_ADDRESS] =
    g_param_spec_uint64 ("address",
                         "Address",
                         "The address of the instruction",
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY] =
    g_param_spec_string ("display",
                         "Display",
                         "Display",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FUNCTION] =
    g_param_spec_string ("function",
                         "Function",
                         "Function",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_instruction_init (IdeDebuggerInstruction *self)
{
}

IdeDebuggerInstruction *
ide_debugger_instruction_new (IdeDebuggerAddress address)
{
  return g_object_new (IDE_TYPE_DEBUGGER_INSTRUCTION,
                       "address", address,
                       NULL);
}

const gchar *
ide_debugger_instruction_get_display (IdeDebuggerInstruction *self)
{
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_INSTRUCTION (self), NULL);

  return priv->display;
}

void
ide_debugger_instruction_set_display (IdeDebuggerInstruction *self,
                                      const gchar            *display)
{
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_INSTRUCTION (self));

  if (g_set_str (&priv->display, display))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY]);
    }
}

const gchar *
ide_debugger_instruction_get_function (IdeDebuggerInstruction *self)
{
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_INSTRUCTION (self), NULL);

  return priv->function;
}

void
ide_debugger_instruction_set_function (IdeDebuggerInstruction *self,
                                      const gchar            *function)
{
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_INSTRUCTION (self));

  if (g_set_str (&priv->function, function))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FUNCTION]);
    }
}

IdeDebuggerAddress
ide_debugger_instruction_get_address (IdeDebuggerInstruction *self)
{
  IdeDebuggerInstructionPrivate *priv = ide_debugger_instruction_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_INSTRUCTION (self), 0);

  return priv->address;
}
