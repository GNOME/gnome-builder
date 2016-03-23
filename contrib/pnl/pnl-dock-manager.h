/* pnl-dock-manager.h
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

#ifndef PNL_DOCK_MANAGER_H
#define PNL_DOCK_MANAGER_H

#include "pnl-dock-types.h"

G_BEGIN_DECLS

struct _PnlDockManagerClass
{
  GObjectClass parent;

  void (*register_dock)   (PnlDockManager *self,
                           PnlDock        *dock);
  void (*unregister_dock) (PnlDockManager *self,
                           PnlDock        *dock);

  void (*padding1) (void);
  void (*padding2) (void);
  void (*padding3) (void);
  void (*padding4) (void);
  void (*padding5) (void);
  void (*padding6) (void);
  void (*padding7) (void);
  void (*padding8) (void);
};

PnlDockManager *pnl_dock_manager_new             (void);
void            pnl_dock_manager_register_dock   (PnlDockManager *self,
                                                  PnlDock        *dock);
void            pnl_dock_manager_unregister_dock (PnlDockManager *self,
                                                  PnlDock        *dock);

G_END_DECLS

#endif /* PNL_DOCK_MANAGER_H */
