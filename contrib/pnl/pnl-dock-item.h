/* pnl-dock-item.h
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

#ifndef PNL_DOCK_ITEM_H
#define PNL_DOCK_ITEM_H

#include "pnl-dock-manager.h"

G_BEGIN_DECLS

struct _PnlDockItemInterface
{
  GTypeInterface parent;

  void            (*set_manager)       (PnlDockItem    *self,
                                        PnlDockManager *manager);
  PnlDockManager *(*get_manager)       (PnlDockItem    *self);
  void            (*manager_set)       (PnlDockItem    *self,
                                        PnlDockManager *old_manager);
  void            (*present_child)     (PnlDockItem    *self,
                                        PnlDockItem    *child);
  void            (*update_visibility) (PnlDockItem    *self);
  gboolean        (*get_child_visible) (PnlDockItem    *self,
                                        PnlDockItem    *child);
  void            (*set_child_visible) (PnlDockItem    *self,
                                        PnlDockItem    *child,
                                        gboolean        child_visible);
};

PnlDockManager *pnl_dock_item_get_manager       (PnlDockItem    *self);
void            pnl_dock_item_set_manager       (PnlDockItem    *self,
                                                 PnlDockManager *manager);
gboolean        pnl_dock_item_adopt             (PnlDockItem    *self,
                                                 PnlDockItem    *child);
void            pnl_dock_item_present           (PnlDockItem    *self);
void            pnl_dock_item_present_child     (PnlDockItem    *self,
                                                 PnlDockItem    *child);
void            pnl_dock_item_update_visibility (PnlDockItem    *self);
gboolean        pnl_dock_item_has_widgets       (PnlDockItem    *self);
gboolean        pnl_dock_item_get_child_visible (PnlDockItem    *self,
                                                 PnlDockItem    *child);
void            pnl_dock_item_set_child_visible (PnlDockItem    *self,
                                                 PnlDockItem    *child,
                                                 gboolean        child_visible);
PnlDockItem    *pnl_dock_item_get_parent        (PnlDockItem    *self);
void            _pnl_dock_item_printf           (PnlDockItem    *self);

G_END_DECLS

#endif /* PNL_DOCK_ITEM_H */
