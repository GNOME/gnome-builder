/* gbp-devhelp-panel.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef GBP_DEVHELP_PANEL_H
#define GBP_DEVHELP_PANEL_H

#include <pnl.h>

G_BEGIN_DECLS

#define GBP_TYPE_DEVHELP_PANEL (gbp_devhelp_panel_get_type())

G_DECLARE_FINAL_TYPE (GbpDevhelpPanel, gbp_devhelp_panel, GBP, DEVHELP_PANEL, PnlDockWidget)

void gbp_devhelp_panel_set_uri      (GbpDevhelpPanel *self,
                                     const gchar     *uri);
void gbp_devhelp_panel_focus_search (GbpDevhelpPanel *self,
                                     const gchar     *keyword);

G_END_DECLS

#endif /* GBP_DEVHELP_PANEL_H */
