/* ide-buffer-change-monitor.h
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

#pragma once

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER_CHANGE_MONITOR (ide_buffer_change_monitor_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeBufferChangeMonitor, ide_buffer_change_monitor, IDE, BUFFER_CHANGE_MONITOR, IdeObject)

typedef enum
{
  IDE_BUFFER_LINE_CHANGE_NONE             = 0,
  IDE_BUFFER_LINE_CHANGE_ADDED            = 1 << 0,
  IDE_BUFFER_LINE_CHANGE_CHANGED          = 1 << 1,
  IDE_BUFFER_LINE_CHANGE_DELETED          = 1 << 2,
  IDE_BUFFER_LINE_CHANGE_PREVIOUS_DELETED = 1 << 3,
} IdeBufferLineChange;

typedef void (*IdeBufferChangeMonitorForeachFunc) (guint               line,
                                                   IdeBufferLineChange change,
                                                   gpointer            user_data);

struct _IdeBufferChangeMonitorClass
{
  IdeObjectClass parent_class;

  void                (*load)           (IdeBufferChangeMonitor            *self,
                                         IdeBuffer                         *buffer);
  IdeBufferLineChange (*get_change)     (IdeBufferChangeMonitor            *self,
                                         guint                              line);
  void                (*reload)         (IdeBufferChangeMonitor            *self);
  void                (*foreach_change) (IdeBufferChangeMonitor            *self,
                                         guint                              line_begin,
                                         guint                              line_end,
                                         IdeBufferChangeMonitorForeachFunc  callback,
                                         gpointer                           user_data);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeBuffer           *ide_buffer_change_monitor_get_buffer     (IdeBufferChangeMonitor            *self);
IDE_AVAILABLE_IN_ALL
void                 ide_buffer_change_monitor_emit_changed   (IdeBufferChangeMonitor            *self);
IDE_AVAILABLE_IN_ALL
void                 ide_buffer_change_monitor_foreach_change (IdeBufferChangeMonitor            *self,
                                                               guint                              line_begin,
                                                               guint                              line_end,
                                                               IdeBufferChangeMonitorForeachFunc  callback,
                                                               gpointer                           user_data);
IDE_AVAILABLE_IN_ALL
IdeBufferLineChange  ide_buffer_change_monitor_get_change     (IdeBufferChangeMonitor            *self,
                                                               guint                              line);
IDE_AVAILABLE_IN_ALL
void                 ide_buffer_change_monitor_reload         (IdeBufferChangeMonitor            *self);

G_END_DECLS
