/* ide-tweaks-variable.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-variable"

#include "config.h"

#include "ide-tweaks-variable.h"

struct _IdeTweaksVariable
{
  IdeTweaksItem parent_instance;

  char *key;
  char *value;
};

G_DEFINE_FINAL_TYPE (IdeTweaksVariable, ide_tweaks_variable, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_KEY,
  PROP_VALUE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeTweaksVariable *
ide_tweaks_variable_new (const char *key,
                         const char *value)
{
  return g_object_new (IDE_TYPE_TWEAKS_VARIABLE,
                       "key", key,
                       "value", value,
                       NULL);
}

static void
ide_tweaks_variable_finalize (GObject *object)
{
  IdeTweaksVariable *self = (IdeTweaksVariable *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->value, g_free);

  G_OBJECT_CLASS (ide_tweaks_variable_parent_class)->finalize (object);
}

static void
ide_tweaks_variable_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTweaksVariable *self = IDE_TWEAKS_VARIABLE (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, ide_tweaks_variable_get_key (self));
      break;

    case PROP_VALUE:
      g_value_set_string (value, ide_tweaks_variable_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_variable_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTweaksVariable *self = IDE_TWEAKS_VARIABLE (object);

  switch (prop_id)
    {
    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_VALUE:
      self->value = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_variable_class_init (IdeTweaksVariableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_tweaks_variable_finalize;
  object_class->get_property = ide_tweaks_variable_get_property;
  object_class->set_property = ide_tweaks_variable_set_property;

  properties [PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_variable_init (IdeTweaksVariable *self)
{
}

const char *
ide_tweaks_variable_get_key (IdeTweaksVariable *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_VARIABLE (self), NULL);

  return self->key;
}

const char *
ide_tweaks_variable_get_value (IdeTweaksVariable *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_VARIABLE (self), NULL);

  return self->value;
}
