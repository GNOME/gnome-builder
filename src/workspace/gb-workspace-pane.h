/* gb-workspace-pane.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_WORKSPACE_PANE_H
#define GB_WORKSPACE_PANE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_WORKSPACE_PANE (gb_workspace_pane_get_type())

G_DECLARE_FINAL_TYPE (GbWorkspacePane, gb_workspace_pane, GB, WORKSPACE_PANE, GtkBin)

GtkWidget       *gb_workspace_pane_new          (void);
GtkPositionType  gb_workspace_pane_get_position (GbWorkspacePane *self);
void             gb_workspace_pane_set_position (GbWorkspacePane *self,
                                                 GtkPositionType  position);
void             gb_workspace_pane_add_page     (GbWorkspacePane *self,
                                                 GtkWidget       *page,
                                                 const gchar     *title,
                                                 const gchar     *icon_name);
void             gb_workspace_pane_remove_page  (GbWorkspacePane *self,
                                                 GtkWidget       *page);

G_END_DECLS

#endif /* GB_WORKSPACE_PANE_H */
