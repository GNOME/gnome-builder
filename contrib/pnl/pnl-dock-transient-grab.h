/* pnl-dock-transient-grab.h
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

#ifndef PNL_DOCK_TRANSIENT_GRAB_H
#define PNL_DOCK_TRANSIENT_GRAB_H

#include "pnl-dock-item.h"

G_BEGIN_DECLS

#define PNL_TYPE_DOCK_TRANSIENT_GRAB (pnl_dock_transient_grab_get_type())

G_DECLARE_FINAL_TYPE (PnlDockTransientGrab, pnl_dock_transient_grab, PNL, DOCK_TRANSIENT_GRAB, GObject)

PnlDockTransientGrab *pnl_dock_transient_grab_new                    (void);
void                  pnl_dock_transient_grab_add_item               (PnlDockTransientGrab *self,
                                                                      PnlDockItem          *item);
void                  pnl_dock_transient_grab_remove_item            (PnlDockTransientGrab *self,
                                                                      PnlDockItem          *item);
void                  pnl_dock_transient_grab_acquire                (PnlDockTransientGrab *self);
void                  pnl_dock_transient_grab_release                (PnlDockTransientGrab *self);
guint                 pnl_dock_transient_grab_get_timeout            (PnlDockTransientGrab *self);
void                  pnl_dock_transient_grab_set_timeout            (PnlDockTransientGrab *self,
                                                                      guint                 timeout);
gboolean              pnl_dock_transient_grab_contains               (PnlDockTransientGrab *self,
                                                                      PnlDockItem          *item);
gboolean              pnl_dock_transient_grab_is_descendant          (PnlDockTransientGrab *self,
                                                                      GtkWidget            *widget);
void                  pnl_dock_transient_grab_steal_common_ancestors (PnlDockTransientGrab *self,
                                                                      PnlDockTransientGrab *other);

G_END_DECLS

#endif /* PNL_DOCK_TRANSIENT_GRAB_H */

