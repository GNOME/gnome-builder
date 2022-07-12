/* ide-gtk.h
 *
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <libide-core.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
gboolean ide_gtk_show_uri_on_window           (GtkWindow        *window,
                                               const char       *uri,
                                               gint64            timestamp,
                                               GError           **error);
IDE_AVAILABLE_IN_ALL
void     ide_gtk_window_present               (GtkWindow         *window);
IDE_AVAILABLE_IN_ALL
void      ide_gtk_progress_bar_start_pulsing  (GtkProgressBar    *progress);
IDE_AVAILABLE_IN_ALL
void      ide_gtk_progress_bar_stop_pulsing   (GtkProgressBar    *progress);
IDE_AVAILABLE_IN_ALL
void      ide_gtk_widget_show_with_fade       (GtkWidget         *widget);
IDE_AVAILABLE_IN_ALL
void      ide_gtk_widget_hide_with_fade       (GtkWidget         *widget);
IDE_AVAILABLE_IN_ALL
void      ide_gtk_list_store_insert_sorted    (GtkListStore      *store,
                                               GtkTreeIter       *iter,
                                               gconstpointer      key,
                                               guint              compare_column,
                                               GCompareDataFunc   compare_func,
                                               gpointer           compare_data);
IDE_AVAILABLE_IN_ALL
void       ide_gtk_widget_destroyed           (GtkWidget         *widget,
                                               GtkWidget        **location);
IDE_AVAILABLE_IN_ALL
char      *ide_g_time_span_to_label           (GTimeSpan          span);
IDE_AVAILABLE_IN_ALL
char      *ide_g_date_time_format_for_display (GDateTime         *self);
IDE_AVAILABLE_IN_ALL
void       ide_gtk_list_view_move_next        (GtkListView       *view);
IDE_AVAILABLE_IN_ALL
void       ide_gtk_list_view_move_previous    (GtkListView       *view);
IDE_AVAILABLE_IN_ALL
gboolean   ide_gtk_list_view_get_selected_row (GtkListView       *view,
                                               guint             *position);
IDE_AVAILABLE_IN_ALL
void       ide_gtk_widget_hide_when_empty     (GtkWidget         *widget,
                                               GListModel        *model);

G_END_DECLS
