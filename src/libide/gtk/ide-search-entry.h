/* ide-search-entry.h
 *
 * Copyright 2021-2022 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SEARCH_ENTRY (ide_search_entry_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSearchEntry, ide_search_entry, IDE, SEARCH_ENTRY, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget *ide_search_entry_new                     (void);
IDE_AVAILABLE_IN_ALL
guint      ide_search_entry_get_occurrence_count    (IdeSearchEntry *self);
IDE_AVAILABLE_IN_ALL
void       ide_search_entry_set_occurrence_count    (IdeSearchEntry *self,
                                                     guint           occurrence_count);
IDE_AVAILABLE_IN_ALL
guint      ide_search_entry_get_occurrence_position (IdeSearchEntry *self);
IDE_AVAILABLE_IN_ALL
void       ide_search_entry_set_occurrence_position (IdeSearchEntry *self,
                                                     int             occurrence_position);

G_END_DECLS
