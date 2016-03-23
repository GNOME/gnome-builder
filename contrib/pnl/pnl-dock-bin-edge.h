/* pnl-dock-bin-edge.h
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

#ifndef PNL_DOCK_BIN_EDGE_H
#define PNL_DOCK_BIN_EDGE_H

#include "pnl-dock-revealer.h"

G_BEGIN_DECLS

#define PNL_TYPE_DOCK_BIN_EDGE (pnl_dock_bin_edge_get_type())

G_DECLARE_DERIVABLE_TYPE (PnlDockBinEdge, pnl_dock_bin_edge, PNL, DOCK_BIN_EDGE, PnlDockRevealer)

struct _PnlDockBinEdgeClass
{
  PnlDockRevealerClass parent;

  void (*move_to_bin_child) (PnlDockBinEdge *self);

  void (*padding1) (void);
  void (*padding2) (void);
  void (*padding3) (void);
  void (*padding4) (void);
  void (*padding5) (void);
  void (*padding6) (void);
  void (*padding7) (void);
  void (*padding8) (void);
};

G_END_DECLS

#endif /* PNL_DOCK_BIN_EDGE_H */
