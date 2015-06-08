/* gb-workbench.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_WORKBENCH_H
#define GB_WORKBENCH_H

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

#define GB_TYPE_WORKBENCH (gb_workbench_get_type())

G_DECLARE_FINAL_TYPE (GbWorkbench, gb_workbench, GB, WORKBENCH, GtkApplicationWindow)

void              gb_workbench_build_async          (GbWorkbench         *self,
                                                     gboolean             force_rebuild,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
gboolean          gb_workbench_build_finish         (GbWorkbench         *self,
                                                     GAsyncResult        *result,
                                                     GError             **error);
IdeContext       *gb_workbench_get_context          (GbWorkbench         *self);
void              gb_workbench_add_temporary_buffer (GbWorkbench         *self);
void              gb_workbench_open                 (GbWorkbench         *self,
                                                     GFile               *file);
void              gb_workbench_open_with_editor     (GbWorkbench         *self,
                                                     GFile               *file);
void              gb_workbench_open_uri_list        (GbWorkbench         *self,
                                                     const gchar * const *uri_list);
gboolean          gb_workbench_get_closing          (GbWorkbench         *self);
void              gb_workbench_views_foreach        (GbWorkbench         *self,
                                                     GtkCallback          callback,
                                                     gpointer             callback_data);
GtkWidget        *gb_workbench_get_workspace        (GbWorkbench         *self);
GtkWidget        *gb_workbench_get_view_grid        (GbWorkbench         *self);

G_END_DECLS

#endif /* GB_WORKBENCH_H */
