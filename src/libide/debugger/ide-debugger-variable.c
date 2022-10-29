/* ide-debugger-variable.c
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

#define G_LOG_DOMAIN "ide-debugger-variable"

#include "config.h"

#include "ide-debugger-variable.h"

typedef struct
{
  gchar *name;
  gchar *type_name;
  gchar *value;
  guint  has_children;
} IdeDebuggerVariablePrivate;

enum {
  PROP_0,
  PROP_HAS_CHILDREN,
  PROP_NAME,
  PROP_TYPE_NAME,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerVariable, ide_debugger_variable, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_variable_finalize (GObject *object)
{
  IdeDebuggerVariable *self = (IdeDebuggerVariable *)object;
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->type_name, g_free);
  g_clear_pointer (&priv->value, g_free);

  G_OBJECT_CLASS (ide_debugger_variable_parent_class)->finalize (object);
}

static void
ide_debugger_variable_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeDebuggerVariable *self = IDE_DEBUGGER_VARIABLE (object);
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HAS_CHILDREN:
      g_value_set_boolean (value, ide_debugger_variable_get_has_children (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_TYPE_NAME:
      g_value_set_string (value, ide_debugger_variable_get_type_name (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, ide_debugger_variable_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_variable_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeDebuggerVariable *self = IDE_DEBUGGER_VARIABLE (object);
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_HAS_CHILDREN:
      ide_debugger_variable_set_has_children (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_TYPE_NAME:
      ide_debugger_variable_set_type_name (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      ide_debugger_variable_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_variable_class_init (IdeDebuggerVariableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_variable_finalize;
  object_class->get_property = ide_debugger_variable_get_property;
  object_class->set_property = ide_debugger_variable_set_property;

  properties [PROP_HAS_CHILDREN] =
    g_param_spec_boolean ("has-children",
                          "Has Children",
                          "If the variable has children variables such as struct members",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the variable",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TYPE_NAME] =
    g_param_spec_string ("type-name",
                         "Type Name",
                         "The name of the variable's type",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE] =
    g_param_spec_string ("value",
                         "Value",
                         "The value of the variable",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_variable_init (IdeDebuggerVariable *self)
{
}

IdeDebuggerVariable *
ide_debugger_variable_new (const gchar *name)
{
  return g_object_new (IDE_TYPE_DEBUGGER_VARIABLE,
                       "name", name,
                       NULL);
}

const gchar *
ide_debugger_variable_get_name (IdeDebuggerVariable *self)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_VARIABLE (self), NULL);

  return priv->name;
}

const gchar *
ide_debugger_variable_get_type_name (IdeDebuggerVariable *self)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_VARIABLE (self), NULL);

  return priv->type_name;
}

void
ide_debugger_variable_set_type_name (IdeDebuggerVariable *self,
                                     const gchar         *type_name)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_VARIABLE (self));

  if (g_set_str (&priv->type_name, type_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TYPE_NAME]);
    }
}

const gchar *
ide_debugger_variable_get_value (IdeDebuggerVariable *self)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_VARIABLE (self), NULL);

  return priv->value;
}

void
ide_debugger_variable_set_value (IdeDebuggerVariable *self,
                                 const gchar         *value)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_VARIABLE (self));

  if (g_set_str (&priv->value, value))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
    }
}

gboolean
ide_debugger_variable_get_has_children (IdeDebuggerVariable *self)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_VARIABLE (self), FALSE);

  return priv->has_children;
}

void
ide_debugger_variable_set_has_children (IdeDebuggerVariable *self,
                                        gboolean             has_children)
{
  IdeDebuggerVariablePrivate *priv = ide_debugger_variable_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_VARIABLE (self));

  has_children = !!has_children;

  if (has_children != priv->has_children)
    {
      priv->has_children = has_children;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_CHILDREN]);
    }
}
