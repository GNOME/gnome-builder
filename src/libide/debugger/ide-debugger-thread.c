/* ide-debugger-thread.c
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

#define G_LOG_DOMAIN "ide-debugger-thread"

#include "config.h"

#include "ide-debugger-thread.h"

typedef struct
{
  gchar *id;
  gchar *group;
} IdeDebuggerThreadPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_GROUP,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerThread, ide_debugger_thread, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_thread_finalize (GObject *object)
{
  IdeDebuggerThread *self = (IdeDebuggerThread *)object;
  IdeDebuggerThreadPrivate *priv = ide_debugger_thread_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->group, g_free);

  G_OBJECT_CLASS (ide_debugger_thread_parent_class)->finalize (object);
}

static void
ide_debugger_thread_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeDebuggerThread *self = IDE_DEBUGGER_THREAD (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_debugger_thread_get_id (self));
      break;

    case PROP_GROUP:
      g_value_set_string (value, ide_debugger_thread_get_group (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_thread_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeDebuggerThread *self = IDE_DEBUGGER_THREAD (object);
  IdeDebuggerThreadPrivate *priv = ide_debugger_thread_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_GROUP:
      ide_debugger_thread_set_group (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_thread_class_init (IdeDebuggerThreadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_thread_finalize;
  object_class->get_property = ide_debugger_thread_get_property;
  object_class->set_property = ide_debugger_thread_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The thread identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_GROUP] =
    g_param_spec_string ("group",
                         "Group",
                         "The thread group, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_thread_init (IdeDebuggerThread *self)
{
}

IdeDebuggerThread *
ide_debugger_thread_new (const gchar *id)
{
  return g_object_new (IDE_TYPE_DEBUGGER_THREAD,
                       "id", id,
                       NULL);
}

const gchar *
ide_debugger_thread_get_id (IdeDebuggerThread *self)
{
  IdeDebuggerThreadPrivate *priv = ide_debugger_thread_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD (self), NULL);

  return priv->id;
}

const gchar *
ide_debugger_thread_get_group (IdeDebuggerThread *self)
{
  IdeDebuggerThreadPrivate *priv = ide_debugger_thread_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD (self), NULL);

  return priv->group;
}

void
ide_debugger_thread_set_group (IdeDebuggerThread *self,
                               const gchar       *group)
{
  IdeDebuggerThreadPrivate *priv = ide_debugger_thread_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (self));

  if (g_set_str (&priv->group, group))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_GROUP]);
    }
}

gint
ide_debugger_thread_compare (IdeDebuggerThread *a,
                             IdeDebuggerThread *b)
{
  IdeDebuggerThreadPrivate *priv_a = ide_debugger_thread_get_instance_private (a);
  IdeDebuggerThreadPrivate *priv_b = ide_debugger_thread_get_instance_private (b);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD (a), 0);
  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD (b), 0);

  if (priv_a->id && priv_b->id)
    {
      if (g_ascii_isdigit (*priv_a->id) && g_ascii_isdigit (*priv_b->id))
        return g_ascii_strtoll (priv_a->id, NULL, 10) -
               g_ascii_strtoll (priv_b->id, NULL, 10);
    }

  return g_strcmp0 (priv_a->id, priv_b->id);
}
