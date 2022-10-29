/* ide-debugger-register.c
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

#define G_LOG_DOMAIN "ide-debugger-register"

#include "config.h"

#include "ide-debugger-register.h"

typedef struct
{
  gchar *id;
  gchar *name;
  gchar *value;
} IdeDebuggerRegisterPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerRegister, ide_debugger_register, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_register_finalize (GObject *object)
{
  IdeDebuggerRegister *self = (IdeDebuggerRegister *)object;
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->value, g_free);

  G_OBJECT_CLASS (ide_debugger_register_parent_class)->finalize (object);
}

static void
ide_debugger_register_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeDebuggerRegister *self = IDE_DEBUGGER_REGISTER (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_debugger_register_get_id (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, ide_debugger_register_get_name (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, ide_debugger_register_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_register_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeDebuggerRegister *self = IDE_DEBUGGER_REGISTER (object);
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_NAME:
      ide_debugger_register_set_name (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      ide_debugger_register_set_value (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_register_class_init (IdeDebuggerRegisterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_register_finalize;
  object_class->get_property = ide_debugger_register_get_property;
  object_class->set_property = ide_debugger_register_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE] =
    g_param_spec_string ("value",
                         "Value",
                         "Value",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_register_init (IdeDebuggerRegister *self)
{
}

IdeDebuggerRegister *
ide_debugger_register_new (const gchar *id)
{
  return g_object_new (IDE_TYPE_DEBUGGER_REGISTER,
                       "id", id,
                       NULL);
}

const gchar *
ide_debugger_register_get_id (IdeDebuggerRegister *self)
{
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_REGISTER (self), NULL);

  return priv->id;
}

const gchar *
ide_debugger_register_get_name (IdeDebuggerRegister *self)
{
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_REGISTER (self), NULL);

  return priv->name;
}

void
ide_debugger_register_set_name (IdeDebuggerRegister *self,
                                const gchar         *name)
{
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_REGISTER (self));

  if (g_set_str (&priv->name, name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

const gchar *
ide_debugger_register_get_value (IdeDebuggerRegister *self)
{
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_REGISTER (self), NULL);

  return priv->value;
}

void
ide_debugger_register_set_value (IdeDebuggerRegister *self,
                                 const gchar         *value)
{
  IdeDebuggerRegisterPrivate *priv = ide_debugger_register_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_REGISTER (self));

  if (g_set_str (&priv->value, value))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
    }
}

gint
ide_debugger_register_compare (IdeDebuggerRegister *a,
                               IdeDebuggerRegister *b)
{
  IdeDebuggerRegisterPrivate *priv_a = ide_debugger_register_get_instance_private (a);
  IdeDebuggerRegisterPrivate *priv_b = ide_debugger_register_get_instance_private (b);

  if (priv_a->id && priv_b->id)
    {
      if (g_ascii_isdigit (*priv_a->id) && g_ascii_isdigit (*priv_b->id))
        return g_ascii_strtoll (priv_a->id, NULL, 10) -
               g_ascii_strtoll (priv_b->id, NULL, 10);
    }

  return g_strcmp0 (priv_a->id, priv_b->id);
}
