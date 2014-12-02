/* gb-source-change-monitor.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SOURCE_CHANGE_MONITOR_H
#define GB_SOURCE_CHANGE_MONITOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_CHANGE_MONITOR            (gb_source_change_monitor_get_type())
#define GB_SOURCE_CHANGE_MONITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CHANGE_MONITOR, GbSourceChangeMonitor))
#define GB_SOURCE_CHANGE_MONITOR_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CHANGE_MONITOR, GbSourceChangeMonitor const))
#define GB_SOURCE_CHANGE_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_CHANGE_MONITOR, GbSourceChangeMonitorClass))
#define GB_IS_SOURCE_CHANGE_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_CHANGE_MONITOR))
#define GB_IS_SOURCE_CHANGE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_CHANGE_MONITOR))
#define GB_SOURCE_CHANGE_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_CHANGE_MONITOR, GbSourceChangeMonitorClass))

typedef struct _GbSourceChangeMonitor        GbSourceChangeMonitor;
typedef struct _GbSourceChangeMonitorClass   GbSourceChangeMonitorClass;
typedef struct _GbSourceChangeMonitorPrivate GbSourceChangeMonitorPrivate;

typedef enum
{
  GB_SOURCE_CHANGE_NONE    = 0,
  GB_SOURCE_CHANGE_ADDED   = 1 << 0,
  GB_SOURCE_CHANGE_CHANGED = 1 << 1,
} GbSourceChangeFlags;

struct _GbSourceChangeMonitor
{
  GObject parent;

  /*< private >*/
  GbSourceChangeMonitorPrivate *priv;
};

struct _GbSourceChangeMonitorClass
{
  GObjectClass parent_class;

  void (*changed) (GbSourceChangeMonitor *monitor);
};

GType                  gb_source_change_monitor_get_type (void) G_GNUC_CONST;
GbSourceChangeMonitor *gb_source_change_monitor_new      (GtkTextBuffer         *buffer);
GFile                 *gb_source_change_monitor_get_file (GbSourceChangeMonitor *monitor);
void                   gb_source_change_monitor_set_file (GbSourceChangeMonitor *monitor,
                                                          GFile                 *file);
GbSourceChangeFlags    gb_source_change_monitor_get_line (GbSourceChangeMonitor *monitor,
                                                          guint                  lineno);
void                   gb_source_change_monitor_reload   (GbSourceChangeMonitor *monitor);

G_END_DECLS

#endif /* GB_SOURCE_CHANGE_MONITOR_H */
