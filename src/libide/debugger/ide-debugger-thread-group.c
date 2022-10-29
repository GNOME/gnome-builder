/* ide-debugger-thread-group.c
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

#define G_LOG_DOMAIN "ide-debugger-thread-group"

#include "config.h"

#include "ide-debugger-thread-group.h"

typedef struct
{
  gchar *id;
  gchar *exit_code;
  gchar *pid;
} IdeDebuggerThreadGroupPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_PID,
  PROP_EXIT_CODE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeDebuggerThreadGroup, ide_debugger_thread_group, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_thread_group_finalize (GObject *object)
{
  IdeDebuggerThreadGroup *self = (IdeDebuggerThreadGroup *)object;
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->exit_code, g_free);
  g_clear_pointer (&priv->pid, g_free);

  G_OBJECT_CLASS (ide_debugger_thread_group_parent_class)->finalize (object);
}

static void
ide_debugger_thread_group_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeDebuggerThreadGroup *self = IDE_DEBUGGER_THREAD_GROUP (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_debugger_thread_group_get_id (self));
      break;

    case PROP_EXIT_CODE:
      g_value_set_string (value, ide_debugger_thread_group_get_exit_code (self));
      break;

    case PROP_PID:
      g_value_set_string (value, ide_debugger_thread_group_get_pid (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_thread_group_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeDebuggerThreadGroup *self = IDE_DEBUGGER_THREAD_GROUP (object);
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_PID:
      ide_debugger_thread_group_set_pid (self, g_value_get_string (value));
      break;

    case PROP_EXIT_CODE:
      ide_debugger_thread_group_set_exit_code (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_thread_group_class_init (IdeDebuggerThreadGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_thread_group_finalize;
  object_class->get_property = ide_debugger_thread_group_get_property;
  object_class->set_property = ide_debugger_thread_group_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The thread group identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_EXIT_CODE] =
    g_param_spec_string ("exit-code",
                         "Exit Code",
                         "The exit code from the process as a string for portability",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PID] =
    g_param_spec_string ("pid",
                         "Pid",
                         "The pid of the thread group",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_thread_group_init (IdeDebuggerThreadGroup *self)
{
}

IdeDebuggerThreadGroup *
ide_debugger_thread_group_new (const gchar *id)
{
  return g_object_new (IDE_TYPE_DEBUGGER_THREAD_GROUP,
                       "id", id,
                       NULL);
}

const gchar *
ide_debugger_thread_group_get_pid (IdeDebuggerThreadGroup *self)
{
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (self), NULL);

  return priv->pid;
}

const gchar *
ide_debugger_thread_group_get_exit_code (IdeDebuggerThreadGroup *self)
{
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (self), NULL);

  return priv->exit_code;
}

void
ide_debugger_thread_group_set_pid (IdeDebuggerThreadGroup *self,
                                   const gchar            *pid)
{
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (self));

  if (g_set_str (&priv->pid, pid))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PID]);
    }
}

void
ide_debugger_thread_group_set_exit_code (IdeDebuggerThreadGroup *self,
                                         const gchar            *exit_code)
{
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (self));

  if (g_set_str (&priv->exit_code, exit_code))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXIT_CODE]);
    }
}

const gchar *
ide_debugger_thread_group_get_id (IdeDebuggerThreadGroup *self)
{
  IdeDebuggerThreadGroupPrivate *priv = ide_debugger_thread_group_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (self), NULL);

  return priv->id;
}

gint
ide_debugger_thread_group_compare (IdeDebuggerThreadGroup *a,
                                   IdeDebuggerThreadGroup *b)
{
  IdeDebuggerThreadGroupPrivate *priv_a = ide_debugger_thread_group_get_instance_private (a);
  IdeDebuggerThreadGroupPrivate *priv_b = ide_debugger_thread_group_get_instance_private (b);

  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (a), 0);
  g_return_val_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (b), 0);

  return g_strcmp0 (priv_a->id, priv_b->id);
}
