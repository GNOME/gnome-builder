/* dspy-name.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "dspy-name"

#include "dspy-name.h"

typedef struct
{
  gchar *name;
  gchar *owner;
  GPid pid;
  guint activatable : 1;
} DspyNamePrivate;

enum {
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_NAME,
  PROP_OWNER,
  PROP_PID,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (DspyName, dspy_name, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
dspy_name_finalize (GObject *object)
{
  DspyName *self = (DspyName *)object;
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->owner, g_free);

  G_OBJECT_CLASS (dspy_name_parent_class)->finalize (object);
}

static void
dspy_name_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  DspyName *self = DSPY_NAME (object);

  switch (prop_id)
    {
    case PROP_ACTIVATABLE:
      g_value_set_boolean (value, dspy_name_get_activatable (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, dspy_name_get_name (self));
      break;

    case PROP_OWNER:
      g_value_set_string (value, dspy_name_get_owner (self));
      break;

    case PROP_PID:
      g_value_set_uint (value, dspy_name_get_pid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  DspyName *self = DSPY_NAME (object);
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ACTIVATABLE:
      priv->activatable = g_value_get_boolean (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_OWNER:
      dspy_name_set_owner (self, g_value_get_string (value));
      break;

    case PROP_PID:
      dspy_name_set_pid (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_name_class_init (DspyNameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dspy_name_finalize;
  object_class->get_property = dspy_name_get_property;
  object_class->set_property = dspy_name_set_property;

  properties [PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          "Activatable",
                          "Activatable",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The peer name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_OWNER] =
    g_param_spec_string ("owner",
                         "Owner",
                         "The owner of the DBus name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PID] =
    g_param_spec_uint ("pid",
                       "Pid",
                       "The pid of the peer",
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
dspy_name_init (DspyName *self)
{
}

DspyName *
dspy_name_new (const gchar *name,
               gboolean     activatable)
{
  return g_object_new (DSPY_TYPE_NAME,
                       "activatable", activatable,
                       "name", name,
                       NULL);
}

gboolean
dspy_name_get_activatable (DspyName *self)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_NAME (self), FALSE);

  return priv->activatable;
}

const gchar *
dspy_name_get_name (DspyName *self)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  return priv->name;
}

void
dspy_name_set_name (DspyName    *self,
                    const gchar *name)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_if_fail (DSPY_IS_NAME (self));

  if (g_strcmp0 (name, priv->name) != 0)
    {
      g_free (priv->name);
      priv->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
    }
}

gint
dspy_name_compare (gconstpointer a,
                   gconstpointer b)
{
  DspyName *item1 = DSPY_NAME ((gpointer)a);
  DspyName *item2 = DSPY_NAME ((gpointer)b);
  const gchar *name1 = dspy_name_get_name (item1);
  const gchar *name2 = dspy_name_get_name (item2);

  if (name1[0] != name2[0])
    {
      if (name1[0] == ':')
        return 1;
      if (name2[0] == ':')
        return -1;
    }

  /* Sort numbers like :1.300 better */
  if (g_str_has_prefix (name1, ":1.") &&
      g_str_has_prefix (name2, ":1."))
    {
      gint i1 = g_ascii_strtoll (name1 + 3, NULL, 10);
      gint i2 = g_ascii_strtoll (name2 + 3, NULL, 10);

      return i1 - i2;
    }

  return g_strcmp0 (name1, name2);
}

GPid
dspy_name_get_pid (DspyName *self)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_NAME (self), 0);

  return priv->pid;
}

void
dspy_name_set_pid (DspyName *self,
                   GPid      pid)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_if_fail (DSPY_IS_NAME (self));

  if (priv->pid != pid)
    {
      priv->pid = pid;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PID]);
    }
}

const gchar *
dspy_name_get_owner (DspyName *self)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_NAME (self), NULL);

  return priv->owner ? priv->owner : priv->name;
}

void
dspy_name_set_owner (DspyName    *self,
                     const gchar *owner)
{
  DspyNamePrivate *priv = dspy_name_get_instance_private (self);

  g_return_if_fail (DSPY_IS_NAME (self));

  if (g_strcmp0 (owner, priv->owner) != 0)
    {
      g_free (priv->owner);
      priv->owner = g_strdup (owner);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OWNER]);
    }
}
