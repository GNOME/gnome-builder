/* pnl-dock-overlay-edge-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef PNL_DOCK_OVERLAY_EDGE_PRIVATE_H
#define PNL_DOCK_OVERLAY_EDGE_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PNL_TYPE_DOCK_OVERLAY_EDGE (pnl_dock_overlay_edge_get_type())

G_DECLARE_FINAL_TYPE (PnlDockOverlayEdge, pnl_dock_overlay_edge, PNL, DOCK_OVERLAY_EDGE, GtkBin)

GtkPositionType pnl_dock_overlay_edge_get_edge     (PnlDockOverlayEdge *self);
void            pnl_dock_overlay_edge_set_edge     (PnlDockOverlayEdge *self,
                                                    GtkPositionType     edge);
gint            pnl_dock_overlay_edge_get_position (PnlDockOverlayEdge *self);
void            pnl_dock_overlay_edge_set_position (PnlDockOverlayEdge *self,
                                                    gint                position);

G_END_DECLS

#endif /* PNL_DOCK_OVERLAY_EDGE_PRIVATE_H */
