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

gboolean         gb_workspace_pane_get_floating            (GbWorkspacePane *self);
guint            gb_workspace_pane_get_transition_duration (GbWorkspacePane *self);
GtkWidget       *gb_workspace_pane_new                     (void);
void             gb_workspace_pane_set_floating            (GbWorkspacePane *self,
                                                            gboolean         floating);
void             gb_workspace_pane_set_transition_duration (GbWorkspacePane *self,
                                                            guint            transition_duration);
GtkPositionType  gb_workspace_pane_get_position            (GbWorkspacePane *self);
void             gb_workspace_pane_set_position            (GbWorkspacePane *self,
                                                            GtkPositionType  position);

G_END_DECLS

#endif /* GB_WORKSPACE_PANE_H */
