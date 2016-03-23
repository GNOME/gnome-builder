/* pnl-dock-revealer.h
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#ifndef PNL_DOCK_REVEALER_H
#define PNL_DOCK_REVEALER_H

#include "pnl-dock-types.h"

G_BEGIN_DECLS

#define PNL_TYPE_DOCK_REVEALER_TRANSITION_TYPE (pnl_dock_revealer_transition_type_get_type())

typedef enum
{
  PNL_DOCK_REVEALER_TRANSITION_TYPE_NONE,
  PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT,
  PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT,
  PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_UP,
  PNL_DOCK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
} PnlDockRevealerTransitionType;

struct _PnlDockRevealerClass
{
  GtkBinClass parent;

  void (*padding1) (void);
  void (*padding2) (void);
  void (*padding3) (void);
  void (*padding4) (void);
  void (*padding5) (void);
  void (*padding6) (void);
  void (*padding7) (void);
  void (*padding8) (void);
};

GType                          pnl_dock_revealer_transition_type_get_type (void);
GtkWidget                     *pnl_dock_revealer_new                      (void);
PnlDockRevealerTransitionType  pnl_dock_revealer_get_transition_type      (PnlDockRevealer *self);
void                           pnl_dock_revealer_set_transition_type      (PnlDockRevealer               *self,
                                                                           PnlDockRevealerTransitionType  transition_type);
gboolean                       pnl_dock_revealer_get_child_revealed       (PnlDockRevealer               *self);
void                           pnl_dock_revealer_set_reveal_child         (PnlDockRevealer               *self,
                                                                           gboolean                       reveal_child);
gboolean                       pnl_dock_revealer_get_reveal_child         (PnlDockRevealer               *self);
gint                           pnl_dock_revealer_get_position             (PnlDockRevealer               *self);
void                           pnl_dock_revealer_set_position             (PnlDockRevealer               *self,
                                                                           gint                           position);
gboolean                       pnl_dock_revealer_get_position_set         (PnlDockRevealer               *self);
void                           pnl_dock_revealer_set_position_set         (PnlDockRevealer               *self,
                                                                           gboolean                       position_set);
guint                          pnl_dock_revealer_get_transition_duration  (PnlDockRevealer               *self);
void                           pnl_dock_revealer_set_transition_duration  (PnlDockRevealer               *self,
                                                                           guint                          transition_duration);

G_END_DECLS

#endif /* PNL_DOCK_REVEALER_H */
