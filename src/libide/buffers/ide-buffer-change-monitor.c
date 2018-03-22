/* ide-buffer-change-monitor.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-buffer-change-monitor"

#include "config.h"

#include <glib/gi18n.h>

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-change-monitor.h"

G_DEFINE_TYPE (IdeBufferChangeMonitor, ide_buffer_change_monitor, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUFFER,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

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
  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->set_buffer)
    IDE_BUFFER_CHANGE_MONITOR_GET_CLASS (self)->set_buffer (self, buffer);
  else
    g_warning ("%s does not implement set_buffer() vfunc",
               g_type_name (G_TYPE_FROM_INSTANCE (self)));
}

void
ide_buffer_change_monitor_emit_changed (IdeBufferChangeMonitor *self)
{
  g_return_if_fail (IDE_IS_BUFFER_CHANGE_MONITOR (self));

  g_signal_emit (self, signals [CHANGED], 0);
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

  object_class->set_property = ide_buffer_change_monitor_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The IdeBuffer to be monitored.",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [CHANGED] = g_signal_new ("changed",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE,
                                    0);
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
