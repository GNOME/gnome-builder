/* ide-buffer-change-monitor.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-buffer-change-monitor"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-marshal.h"

#include "ide-buffer.h"
#include "ide-buffer-change-monitor.h"
#include "ide-buffer-private.h"

typedef struct
{
  IdeBuffer *buffer;
} IdeBufferChangeMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBufferChangeMonitor, ide_buffer_change_monitor, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

IdeBufferLineChange
ide_buffer_change_monitor_get_change (IdeBufferChangeMonitor *self,
                                      guint                   line)
{
  g_return_val_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self), IDE_BUFFER_LINE_CHANGE_NONE);

  if G_LIKELY (IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->get_change)
    return IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->get_change (self, line);
  else
    return IDE_BUFFER_LINE_CHANGE_NONE;
}

static void
ide_buffer_change_monitor_set_buffer (IdeBufferChangeMonitor *self,
                                      IdeBuffer              *buffer)
{
  IdeBufferChangeMonitorPrivate *priv = ide_buffer_change_monitor_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  priv->buffer = g_object_ref (buffer);

  if (IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->load)
    IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->load (self, buffer);
}

void
ide_buffer_change_monitor_emit_changed (IdeBufferChangeMonitor *self)
{
  IdeBufferChangeMonitorPrivate *priv = ide_buffer_change_monitor_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));

  g_signal_emit (self, signals [CHANGED], 0);

  if (priv->buffer)
    _ide_buffer_line_flags_changed (priv->buffer);
}

static void
ide_buffer_change_monitor_destroy (IdeObject *object)
{
  IdeBufferChangeMonitor *self = (IdeBufferChangeMonitor *)object;
  IdeBufferChangeMonitorPrivate *priv = ide_buffer_change_monitor_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (self));

  g_clear_object (&priv->buffer);

  IDE_OBJECT_CLASS (ide_buffer_change_monitor_parent_class)->destroy (object);
}

static void
ide_buffer_change_monitor_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdeBufferChangeMonitor *self = IDE_BUFFER_CHANGE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_buffer_change_monitor_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_change_monitor_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeBufferChangeMonitor *self = IDE_BUFFER_CHANGE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_buffer_change_monitor_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_change_monitor_class_init (IdeBufferChangeMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_buffer_change_monitor_get_property;
  object_class->set_property = ide_buffer_change_monitor_set_property;

  i_object_class->destroy = ide_buffer_change_monitor_destroy;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The IdeBuffer to be monitored.",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);
}

static void
ide_buffer_change_monitor_init (IdeBufferChangeMonitor *self)
{
}

void
ide_buffer_change_monitor_reload (IdeBufferChangeMonitor *self)
{
  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));

  if (IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->reload)
    IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->reload (self);
}

/**
 * ide_buffer_change_monitor_foreach_change:
 * @self: a #IdeBufferChangeMonitor
 * @line_begin: the starting line
 * @line_end: the end line
 * @callback: (scope call): a callback
 * @user_data: user data for @callback
 *
 * Calls @callback for every line between @line_begin and @line_end that have
 * an addition, deletion, or change.
 */
void
ide_buffer_change_monitor_foreach_change (IdeBufferChangeMonitor            *self,
                                          guint                              line_begin,
                                          guint                              line_end,
                                          IdeBufferChangeMonitorForeachFunc  callback,
                                          gpointer                           user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (callback != NULL);

  if (IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->foreach_change)
    IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->foreach_change (self, line_begin, line_end, callback, user_data);
}

/**
 * ide_buffer_change_monitor_get_buffer:
 * @self: a #IdeBufferChangeMonitor
 *
 * Gets the #IdeBufferChangeMonitor:buffer property.
 *
 * Returns: (transfer none): an #IdeBuffer
 */
IdeBuffer *
ide_buffer_change_monitor_get_buffer (IdeBufferChangeMonitor *self)
{
  IdeBufferChangeMonitorPrivate *priv = ide_buffer_change_monitor_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self), NULL);

  return priv->buffer;
}
