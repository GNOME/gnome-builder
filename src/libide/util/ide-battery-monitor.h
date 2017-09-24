/* ide-battery-monitor.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_BATTERY_MONITOR_H
#define IDE_BATTERY_MONITOR_H

#include <glib.h>

G_BEGIN_DECLS

gdouble  ide_battery_monitor_get_energy_percentage (void);
gboolean ide_battery_monitor_get_on_battery        (void);
gboolean ide_battery_monitor_get_should_conserve   (void);
void     _ide_battery_monitor_init                 (void) G_GNUC_INTERNAL;
void     _ide_battery_monitor_shutdown             (void) G_GNUC_INTERNAL;

G_END_DECLS

#endif /* IDE_BATTERY_MONITOR_H */
