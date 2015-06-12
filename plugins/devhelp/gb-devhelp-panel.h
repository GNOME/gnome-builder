/* gb-devhelp-panel.h
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

#ifndef GB_DEVHELP_PANEL_H
#define GB_DEVHELP_PANEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_DEVHELP_PANEL (gb_devhelp_panel_get_type())

G_DECLARE_FINAL_TYPE (GbDevhelpPanel, gb_devhelp_panel, GB, DEVHELP_PANEL, GtkBin)

void gb_devhelp_panel_set_uri (GbDevhelpPanel *self,
                               const gchar    *uri);

G_END_DECLS

#endif /* GB_DEVHELP_PANEL_H */
