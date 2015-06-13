/* gb-device-manager-panel.h
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

#ifndef GB_DEVICE_MANAGER_PANEL_H
#define GB_DEVICE_MANAGER_PANEL_H

#include <gtk/gtk.h>

#include "gb-workbench-addin.h"

G_BEGIN_DECLS

#define GB_TYPE_DEVICE_MANAGER_PANEL (gb_device_manager_panel_get_type())

G_DECLARE_FINAL_TYPE (GbDeviceManagerPanel, gb_device_manager_panel,
                      GB, DEVICE_MANAGER_PANEL,
                      GtkBox)

G_END_DECLS

#endif /* GB_DEVICE_MANAGER_PANEL_H */
