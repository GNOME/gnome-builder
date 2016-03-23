/* pnl-tab.h
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

#ifndef PNL_TAB_H
#define PNL_TAB_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PNL_TYPE_TAB (pnl_tab_get_type())

G_DECLARE_FINAL_TYPE (PnlTab, pnl_tab, PNL, TAB, GtkToggleButton)

const gchar     *pnl_tab_get_title  (PnlTab          *self);
void             pnl_tab_set_title  (PnlTab          *self,
                                     const gchar     *title);
GtkPositionType  pnl_tab_get_edge   (PnlTab          *self);
void             pnl_tab_set_edge   (PnlTab          *self,
                                     GtkPositionType  edge);
GtkWidget       *pnl_tab_get_widget (PnlTab          *self);
void             pnl_tab_set_widget (PnlTab          *self,
                                     GtkWidget       *widget);

G_END_DECLS

#endif /* PNL_TAB_H */
