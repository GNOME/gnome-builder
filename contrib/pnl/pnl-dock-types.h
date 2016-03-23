/* pnl-dock-types.h
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

#if !defined(PNL_INSIDE) && !defined(PNL_COMPILATION)
# error "Only <pnl.h> can be included directly."
#endif

#ifndef PNL_TYPES_H
#define PNL_TYPES_H

#include <gtk/gtk.h>

#include "pnl-multi-paned.h"

G_BEGIN_DECLS

#define PNL_TYPE_DOCK          (pnl_dock_get_type ())
#define PNL_TYPE_DOCK_BIN      (pnl_dock_bin_get_type())
#define PNL_TYPE_DOCK_ITEM     (pnl_dock_item_get_type())
#define PNL_TYPE_DOCK_MANAGER  (pnl_dock_manager_get_type())
#define PNL_TYPE_DOCK_OVERLAY  (pnl_dock_overlay_get_type())
#define PNL_TYPE_DOCK_PANED    (pnl_dock_paned_get_type())
#define PNL_TYPE_DOCK_REVEALER (pnl_dock_revealer_get_type())
#define PNL_TYPE_DOCK_STACK    (pnl_dock_stack_get_type())
#define PNL_TYPE_DOCK_WIDGET   (pnl_dock_widget_get_type())
#define PNL_TYPE_DOCK_WINDOW   (pnl_dock_window_get_type())

G_DECLARE_INTERFACE      (PnlDock,         pnl_dock,          PNL, DOCK,          GtkContainer)
G_DECLARE_DERIVABLE_TYPE (PnlDockBin,      pnl_dock_bin,      PNL, DOCK_BIN,      GtkContainer)
G_DECLARE_INTERFACE      (PnlDockItem,     pnl_dock_item,     PNL, DOCK_ITEM,     GtkWidget)
G_DECLARE_DERIVABLE_TYPE (PnlDockManager,  pnl_dock_manager,  PNL, DOCK_MANAGER,  GObject)
G_DECLARE_DERIVABLE_TYPE (PnlDockOverlay,  pnl_dock_overlay,  PNL, DOCK_OVERLAY,  GtkEventBox)
G_DECLARE_DERIVABLE_TYPE (PnlDockPaned,    pnl_dock_paned,    PNL, DOCK_PANED,    PnlMultiPaned)
G_DECLARE_DERIVABLE_TYPE (PnlDockRevealer, pnl_dock_revealer, PNL, DOCK_REVEALER, GtkBin)
G_DECLARE_DERIVABLE_TYPE (PnlDockStack,    pnl_dock_stack,    PNL, DOCK_STACK,    GtkBox)
G_DECLARE_DERIVABLE_TYPE (PnlDockWidget,   pnl_dock_widget,   PNL, DOCK_WIDGET,   GtkBin)
G_DECLARE_DERIVABLE_TYPE (PnlDockWindow,   pnl_dock_window,   PNL, DOCK_WINDOW,   GtkWindow)

G_END_DECLS

#endif /* PNL_TYPES_H */
