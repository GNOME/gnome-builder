/* pnl-multi-paned.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(PNL_INSIDE) && !defined(PNL_COMPILATION)
# error "Only <pnl.h> can be included directly."
#endif

#ifndef PNL_MULTI_PANED_H
#define PNL_MULTI_PANED_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PNL_TYPE_MULTI_PANED (pnl_multi_paned_get_type())

G_DECLARE_DERIVABLE_TYPE (PnlMultiPaned, pnl_multi_paned, PNL, MULTI_PANED, GtkContainer)

struct _PnlMultiPanedClass
{
  GtkContainerClass parent;

  void (*resize_drag_begin) (PnlMultiPaned *self,
                             GtkWidget     *child);
  void (*resize_drag_end)   (PnlMultiPaned *self,
                             GtkWidget     *child);

  void (*padding1) (void);
  void (*padding2) (void);
  void (*padding3) (void);
  void (*padding4) (void);
  void (*padding5) (void);
  void (*padding6) (void);
  void (*padding7) (void);
  void (*padding8) (void);
};

GtkWidget *pnl_multi_paned_new            (void);
guint      pnl_multi_paned_get_n_children (PnlMultiPaned *self);

G_END_DECLS

#endif /* PNL_MULTI_PANED_H */
